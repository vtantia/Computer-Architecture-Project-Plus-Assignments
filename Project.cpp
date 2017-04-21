#include "pin.H"
#include "xed-category-enum.h"
#include <fstream>
#include <iostream>
#define bitI(value, i) ((value >> i) & 1)

void Exit();

struct BtbEntry {
  BOOL valid;
  ADDRINT tag, target;
  UINT64 timestamp;
};

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 totPreds = 0;
UINT64 directionCount[2] = {0};

UINT64 fast_forward_count = 30;
UINT64 run_inst_count = 1000000000;

UINT32 mispredFNBT[3] = {0};
//
UINT32 mispredBimod[3] = {0};
UINT32 bimod[512] = {0};
//
UINT32 sag_bht[1024] = {0};
UINT32 sag_pht[2048] = {0};
UINT32 mispredSag[3] = {0};
//
UINT32 g_bht = 0;
UINT32 gag_pht[4096] = {0};
UINT32 mispredGag[3] = {0};
//
UINT32 gshare_pht[4096] = {0};
UINT32 mispredGshare[3] = {0};
//
UINT32 mispredHyb1[3] = {0};
UINT32 hyb1_sag_bht[1024] = {0};
UINT32 hyb1_sag_pht[512] = {0};
UINT32 hyb1_gag_bht = 0;
UINT32 hyb1_gag_pht[512] = {0};
UINT32 hyb1_meta[512] = {0};
//
UINT32 mispredHyb2maj[3] = {0};
//
UINT32 mispredHyb2meta[3] = {0};
UINT32 hyb2_s_g[512] = {0};
UINT32 hyb2_s_gsh[512] = {0};
UINT32 hyb2_g_gsh[512] = {0};

#define NUM_BTB 128
UINT32 mispredBTB[2] = {0};
UINT64 lastTimeBTB[2] = {0};
UINT32 btbHist = 0;
BtbEntry btb[2][NUM_BTB][4];
UINT32 totalpredBTB[2] = {0};
UINT32 missesBTB[2] = {0};

// For Project
// TODO:
// 1. Analyze how this will work in hardware in terms of speed.
// 2. Analyze how it can be used in other ways to improve branch
// prediction.
// 3. Try for various storage budgets. Ideally find an empirical formula
//    for improvement/mispredictions in terms of storage budget.

// GAS hash size
#define G_HASH_SIZE 512

// GAS width of history
#define G_WIDTH 8

// SAS hash size
#define S_HASH_SIZE 512

// SAS width of history
#define S_WIDTH 7

// Full hash size
#define FH_HASH_SIZE 256

#define FH_WIDTH (S_WIDTH + G_WIDTH)

INT32 g_weights[G_HASH_SIZE][G_WIDTH] = {0};
INT32 s_weights[S_HASH_SIZE][S_WIDTH] = {0};
INT32 fh_weights[FH_HASH_SIZE][FH_WIDTH] = {0};

UINT32 sp_bht[S_HASH_SIZE] = {0};
UINT32 g_mispredPerceptron[3] = {0};
UINT32 s_mispredPerceptron[3] = {0};
UINT32 fh_mispredPerceptron[3] = {0};

UINT32 p_mispredHyb[3] = {0};
UINT32 gp_hist = 0;
UINT32 hyb1_pmeta[MIN(1 << G_WIDTH, 1<<10)] = {0};

std::ostream *out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "",
                            "specify file name for HW1 output");

KNOB<string> KnobFastForwardCount(KNOB_MODE_WRITEONCE, "pintool", "f", "",
                                  "specify fast forward count for HW1");

KNOB<BOOL>
    KnobCount(KNOB_MODE_WRITEONCE, "pintool", "count", "1",
              "count instructions, basic blocks  in the application");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage() {
  cerr << "This tool prints out the number of dynamically executed "
       << endl
       << "instructions, basic blocks  in the application." << endl
       << endl;
  cerr << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

inline VOID updateCount(UINT32 *toChange, BOOL isTaken, UINT32 limit) {
  if (*toChange != limit && isTaken)
    (*toChange)++;
  else if (*toChange != 0 && !isTaken)
    (*toChange)--;
}

inline VOID updateCountSigned(INT32 *toChange, INT32 inc, INT32 limit) {
  *toChange += inc;
  if (*toChange > limit)
    (*toChange) = limit;
  else if (*toChange < -limit)
    (*toChange) = -limit;
}

VOID BranchPred(ADDRINT insAddr, ADDRINT bTargetAddr, BOOL isTaken) {
  BOOL isForward = (bTargetAddr > insAddr);
  btbHist = (btbHist << 1) % NUM_BTB;
  btbHist += isTaken;

  INT32 dir = (isTaken ? 1 : -1);
  INT32 g_out = 0;
  for (UINT32 i = 0; i < G_WIDTH; i++) {
    g_out += bitI(gp_hist, i) * g_weights[insAddr % G_HASH_SIZE][i];
  }
  UINT32 gp_pred = (g_out >= 0);
  UINT32 gp_isMisPred = ((g_out * dir) < 0);
  g_mispredPerceptron[0] += isForward * gp_isMisPred;
  g_mispredPerceptron[1] += (!isForward) * gp_isMisPred;
  g_mispredPerceptron[2] += gp_isMisPred;
  if ((g_out * dir) < 0 || (g_out < 15 && g_out > -15)) {
    for (UINT32 i = 0; i < G_WIDTH; i++) {
      updateCountSigned(&g_weights[insAddr % G_HASH_SIZE][i],
                        dir * bitI(gp_hist, i), 7);
    }
  }

  UINT32 sp_hist = sp_bht[insAddr % S_HASH_SIZE];
  INT32 s_out = 0;
  for (UINT32 i = 0; i < S_WIDTH; i++) {
    s_out += bitI(sp_hist, i) * s_weights[insAddr % S_HASH_SIZE][i];
  }
  UINT32 sp_pred = (s_out >= 0);
  UINT32 sp_isMisPred = ((s_out * dir) < 0);
  s_mispredPerceptron[0] += isForward * sp_isMisPred;
  s_mispredPerceptron[1] += (!isForward) * sp_isMisPred;
  s_mispredPerceptron[2] += sp_isMisPred;
  if ((s_out * dir) < 0 || (s_out < 15 && s_out > -15)) {
    for (UINT32 i = 0; i < S_WIDTH; i++) {
      updateCountSigned(&s_weights[insAddr % S_HASH_SIZE][i],
                        dir * bitI(sp_hist, i), 7);
    }
  }

  UINT32 p_needUpdate = (gp_pred != sp_pred);
  p_mispredHyb[0] +=
      isForward *
      ((hyb1_pmeta[gp_hist & 0x3ff] < 2) ? sp_isMisPred : gp_isMisPred);
  p_mispredHyb[1] +=
      (!isForward) *
      ((hyb1_pmeta[gp_hist & 0x3ff] < 2) ? sp_isMisPred : gp_isMisPred);
  p_mispredHyb[2] +=
      (hyb1_pmeta[gp_hist & 0x3ff] < 2) ? sp_isMisPred : gp_isMisPred;
  if (p_needUpdate) {
    // GAg is assumed on high end of counter
    UINT32 gpCorrect = (!gp_isMisPred);
    updateCount(&hyb1_pmeta[gp_hist & 0x3ff], gpCorrect, 3);
  }

  INT32 fh_out = 0;
  for (UINT32 i = 0; i < S_WIDTH; i++) {
    fh_out += bitI(sp_hist, i) * fh_weights[insAddr % FH_HASH_SIZE][i];
  }
  for (UINT32 i = 0; i < G_WIDTH; i++) {
    fh_out +=
        bitI(gp_hist, i) * fh_weights[insAddr % FH_HASH_SIZE][i + S_WIDTH];
  }

  UINT32 fh_isMisPred = ((fh_out * dir) < 0);
  fh_mispredPerceptron[0] += isForward * fh_isMisPred;
  fh_mispredPerceptron[1] += (!isForward) * fh_isMisPred;
  fh_mispredPerceptron[2] += fh_isMisPred;

  if ((fh_out * dir) < 0 || (fh_out < 15 && fh_out > -15)) {
    for (UINT32 i = 0; i < S_WIDTH; i++) {
      updateCountSigned(&fh_weights[insAddr % FH_HASH_SIZE][i],
                        dir * bitI(sp_hist, i), 7);
    }
    for (UINT32 i = 0; i < G_WIDTH; i++) {
      updateCountSigned(&fh_weights[insAddr % FH_HASH_SIZE][i + S_WIDTH],
                        dir * bitI(gp_hist, i), 7);
    }
  }

  gp_hist = ((gp_hist << 1) + isTaken) & ((1 << G_WIDTH) - 1);
  sp_bht[insAddr % S_HASH_SIZE] =
      ((sp_bht[insAddr % S_HASH_SIZE] << 1) + isTaken) &
      ((1 << S_WIDTH) - 1);

  totPreds++;
  directionCount[isForward]++;

  mispredFNBT[0] += isForward * (isForward ? isTaken : !isTaken);
  mispredFNBT[1] += (!isForward) * (isForward ? isTaken : !isTaken);
  mispredFNBT[2] += isForward ? isTaken : !isTaken;

  mispredBimod[0] +=
      isForward * ((bimod[(insAddr) % 5500] < 2) ? isTaken : !isTaken);
  mispredBimod[1] +=
      (!isForward) * ((bimod[(insAddr) % 5500] < 2) ? isTaken : !isTaken);
  mispredBimod[2] += (bimod[(insAddr) % 5500] < 2) ? isTaken : !isTaken;
  updateCount(&bimod[(insAddr) % 5500], isTaken, 7);

  UINT32 hist_sag = sag_bht[insAddr % 1024];
  UINT32 sag_pred = !(sag_pht[hist_sag] < 2);
  mispredSag[0] += isForward * ((sag_pred) ? !isTaken : isTaken);
  mispredSag[1] += (!isForward) * ((sag_pred) ? !isTaken : isTaken);
  mispredSag[2] += (sag_pred) ? !isTaken : isTaken;
  updateCount(&sag_pht[hist_sag], isTaken, 3);
  sag_bht[insAddr % 1024] =
      ((sag_bht[insAddr % 1024] << 1) + isTaken) & 0x7ff;

  UINT32 gag_pred = !(gag_pht[g_bht] < 4);
  mispredGag[0] += isForward * ((gag_pred) ? !isTaken : isTaken);
  mispredGag[1] += (!isForward) * ((gag_pred) ? !isTaken : isTaken);
  mispredGag[2] += (gag_pred) ? !isTaken : isTaken;
  updateCount(&gag_pht[g_bht], isTaken, 7);

  UINT32 index = g_bht ^ (insAddr & 0x1ff);
  UINT32 gshare_pred = !(gshare_pht[index] < 4);
  mispredGshare[0] += isForward * ((gshare_pred) ? !isTaken : isTaken);
  mispredGshare[1] += (!isForward) * ((gshare_pred) ? !isTaken : isTaken);
  mispredGshare[2] += (gshare_pred) ? !isTaken : isTaken;
  updateCount(&gshare_pht[index], isTaken, 7);

 // UINT32 needUpdate = (gag_pred != sag_pred);
 // mispredHyb1[0] +=
 //     isForward * ((hyb1_meta[g_bht] < 2) ? (isTaken != sag_pred)
 //                                         : (isTaken != gag_pred));
 // mispredHyb1[1] +=
 //     (!isForward) * ((hyb1_meta[g_bht] < 2) ? (isTaken != sag_pred)
 //                                            : (isTaken != gag_pred));
 // mispredHyb1[2] += (hyb1_meta[g_bht] < 2) ? (isTaken != sag_pred)
 //                                          : (isTaken != gag_pred);
 // if (needUpdate) {
 //   // GAg is assumed on high end of counter
 //   UINT32 gagCorrect = (gag_pred == isTaken);
 //   updateCount(&hyb1_meta[g_bht], gagCorrect, 3);
 // }

 // UINT32 majVote = (gshare_pred + gag_pred + sag_pred) > 1;
 // mispredHyb2maj[0] += isForward * ((majVote != isTaken));
 // mispredHyb2maj[1] += (!isForward) * ((majVote != isTaken));
 // mispredHyb2maj[2] += (majVote != isTaken);

 // UINT32 metaVote = 0;
 // if (hyb2_s_g[g_bht] < 2 && hyb2_s_gsh[g_bht] < 2) {
 //   metaVote = sag_pred;
 // } else if (hyb2_g_gsh[g_bht] > 1 && hyb2_s_gsh[g_bht] > 1) {
 //   metaVote = gshare_pred;
 // } else if (hyb2_g_gsh[g_bht] < 2 && hyb2_s_g[g_bht] > 1) {
 //   metaVote = gag_pred;
 // } else {
 //   metaVote = (gshare_pred + gag_pred + sag_pred) > 1;
 // }
 // mispredHyb2meta[0] += isForward * ((metaVote != isTaken));
 // mispredHyb2meta[1] += (!isForward) * ((metaVote != isTaken));
 // mispredHyb2meta[2] += (metaVote != isTaken);
 // UINT32 update_s_g = ((!gag_pred) * sag_pred) + (gag_pred * (!sag_pred));
 // UINT32 update_s_gsh =
 //     ((!gshare_pred) * sag_pred) + (gshare_pred * (!sag_pred));
 // UINT32 update_g_gsh =
 //     ((!gag_pred) * gshare_pred) + (gag_pred * (!gshare_pred));
 // if (update_s_g) // Second is higher
 //   updateCount(&hyb2_s_g[g_bht], (gag_pred == isTaken), 3);
 // if (update_s_gsh) // Second is higher
 //   updateCount(&hyb2_s_gsh[g_bht], (gshare_pred == isTaken), 3);
 // if (update_g_gsh) // Second is higher
 //   updateCount(&hyb2_g_gsh[g_bht], (gshare_pred == isTaken), 3);

  g_bht = ((g_bht << 1) + isTaken) & 0xfff;
}

VOID UpdateCondBrHist(BOOL isTaken) {}

VOID IndirectPred(ADDRINT insAddr, ADDRINT nextInsAddr, ADDRINT targetAddr,
                  UINT32 cacheIndex, UINT32 btbIndex) {
  UINT32 invalidIndex = 4, lruIndex = 4;
  BOOL found = false;
  totalpredBTB[btbIndex]++;

  for (UINT32 i = 0; i < 4; i++) {
    BtbEntry &b = btb[btbIndex][cacheIndex][i];

    if (!b.valid) {
      invalidIndex = i;
    } else if (b.tag == insAddr) {
      mispredBTB[btbIndex] += (targetAddr != b.target);
      b.target = targetAddr;
      b.timestamp = lastTimeBTB[btbIndex]++;
      found = true;
      break;
    } else if (lruIndex == 4 ||
               b.timestamp <
                   btb[btbIndex][cacheIndex][lruIndex].timestamp) {
      lruIndex = i;
    }
  }

  UINT32 toUpdate = (invalidIndex == 4) ? lruIndex : invalidIndex;
  missesBTB[btbIndex] += !found;
  if (!found && targetAddr != nextInsAddr) {
    mispredBTB[btbIndex] += 1;
    btb[btbIndex][cacheIndex][toUpdate] =
        (BtbEntry){true, insAddr, targetAddr, lastTimeBTB[btbIndex]++};
  }
}

VOID IndirectPredBTB(ADDRINT insAddr, ADDRINT nextInsAddr,
                     ADDRINT targetAddr) {
  UINT32 cacheIndex = (insAddr % NUM_BTB);
  IndirectPred(insAddr, nextInsAddr, targetAddr, cacheIndex, 0);

  cacheIndex = cacheIndex ^ (btbHist);
  IndirectPred(insAddr, nextInsAddr, targetAddr, cacheIndex, 1);
}

ADDRINT Terminate(void) {
  return (insCount >= fast_forward_count + run_inst_count);
}

// Analysis routine to check fast-forward condition
ADDRINT FastForward(void) { return (insCount >= fast_forward_count); }

/* =====================================================================
 */
// Analysis routines
/* =====================================================================
 */

/* =====================================================================
 */
// Instrumentation callbacks
/* =====================================================================
 */

VOID Instruction(INS ins, VOID *v) {
  if (INS_Category(ins) == XED_CATEGORY_COND_BR) {
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)BranchPred, IARG_ADDRINT,
        INS_Address(ins),        // instruction address
        IARG_BRANCH_TARGET_ADDR, // branch target address
        IARG_BRANCH_TAKEN,       // if branch is taken
        IARG_END);
  }

  if (INS_IsCall(ins) && INS_IsIndirectBranchOrCall(ins)) {
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)IndirectPredBTB, IARG_ADDRINT,
        INS_Address(ins),                   // instruction address
        IARG_ADDRINT, INS_NextAddress(ins), // next ins addr
        IARG_BRANCH_TARGET_ADDR,            // branch target address
        IARG_END);
  }

  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Exit, IARG_END);
}

VOID CountBbl(UINT32 numInstInBbl) { insCount += numInstInBbl; }

VOID Trace(TRACE trace, VOID *v) {
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl);
       bbl = BBL_Next(bbl)) {
    BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl, IARG_UINT32,
                   BBL_NumIns(bbl), IARG_END);
  }
}

void outputBTB(UINT32 index) {
  *out << "Mispredictions: " << mispredBTB[index] << endl;
  *out << "Ratio of mispredictions: "
       << ((double)mispredBTB[index]) / totalpredBTB[index] << endl;
  *out << "BTB misses: " << missesBTB[index] << endl;
  *out << "Ratio of BTB misses: "
       << ((double)missesBTB[index]) / totalpredBTB[index] << endl;
  *out << "Total predictions: " << totalpredBTB[index] << endl;
}

void Exit() {
  *out << "Fast forward count: " << fast_forward_count << endl;
  *out << "===============================================" << endl;
  *out << "MyPinTool analysis results: " << endl;
  *out << "Number of instructions: " << insCount << endl;
  *out << "===============================================" << endl;
  *out << "Total number of predictions tried: " << totPreds << endl;
  *out << "Total number of predictions tried for forward: "
       << directionCount[1] << endl;
  *out << "Total number of predictions tried for backward: "
       << directionCount[0] << endl;

  *out << "G_HASH_SIZE: " << G_HASH_SIZE << endl;
  *out << "G_WIDTH:" << G_WIDTH << endl;
  *out << "S_HASH_SIZE: " << S_HASH_SIZE << endl;
  *out << "S_WIDTH:" << S_WIDTH << endl;
  *out << "FH_HASH_SIZE: " << FH_HASH_SIZE << endl;
  *out << "FH_WIDTH:" << FH_WIDTH << endl;

  *out << " & FNBT & Bimodal & SAg & GAg & gshare & Hybrid of SAg and GAg "
          "& Hybrid of All: Majority Voter & Hybrid of All: Three Tables"
       << endl;
  *out << "Ratio of mispredictions & " << mispredFNBT << " & "
       << mispredBimod << " & " << mispredSag << " & " << mispredGag
       << " & " << mispredGshare << " & " << mispredHyb1 << " & "
       << mispredHyb2maj << " & " << mispredHyb2meta << endl;
  *out << "===============================================" << endl;
  *out << "Mispredictions in G-Perceptron : " << g_mispredPerceptron[0]
       << " " << g_mispredPerceptron[1] << " " << g_mispredPerceptron[2]
       << endl;
  *out << "Mispredictions in S-Perceptron : " << s_mispredPerceptron[0]
       << " " << s_mispredPerceptron[1] << " " << s_mispredPerceptron[2]
       << endl;
  *out << "Mispredictions in FH-Perceptron : " << fh_mispredPerceptron[0]
       << " " << fh_mispredPerceptron[1] << " " << fh_mispredPerceptron[2]
       << endl;
  *out << "Mispredictions in meta-predictor for G-Perceptron and "
          "S-perceptron: "
       << p_mispredHyb[0] << " " << p_mispredHyb[1] << " "
       << p_mispredHyb[2] << endl;
  *out << "Mispredictions in meta-predictor for G-Perceptron and "
          "S-perceptron: "
       << p_mispredHyb[0] << " " << p_mispredHyb[1] << " "
       << p_mispredHyb[2] << endl;
  *out << "Mispredictions in FNBT : " << mispredFNBT[0] << " "
       << mispredFNBT[1] << " " << mispredFNBT[2] << endl;
  *out << "Mispredictions in bimodal : " << mispredBimod[0] << " "
       << mispredBimod[1] << " " << mispredBimod[2] << endl;
  *out << "Mispredictions in SAg : " << mispredSag[0] << " "
       << mispredSag[1] << " " << mispredSag[2] << endl;
  *out << "Mispredictions in GAg : " << mispredGag[0] << " "
       << mispredGag[1] << " " << mispredGag[2] << endl;
  *out << "Mispredictions in Gshare : " << mispredGshare[0] << " "
       << mispredGshare[1] << " " << mispredGshare[2] << endl;
  *out << "Mispredictions in Hybrid1 (SAg+GAg): " << mispredHyb1[0] << " "
       << mispredHyb1[1] << " " << mispredHyb1[2] << endl;
  *out << "Mispredictions in Hybrid2-1 (SAg+GAg+gshare, majority): "
       << mispredHyb2maj[0] << " " << mispredHyb2maj[1] << " "
       << mispredHyb2maj[2] << endl;
  *out << "Mispredictions in Hybrid2-2 (SAg+GAg+gshare, metameta): "
       << mispredHyb2meta[0] << " " << mispredHyb2meta[1] << " "
       << mispredHyb2meta[2] << endl;
  *out << "===============================================" << endl;
  *out << "BTB PC:" << endl;
  outputBTB(0);
  *out << "===============================================" << endl;
  *out << "BTB PC and global history:" << endl;
  outputBTB(1);
  *out << "===============================================" << endl;
  *out << " & BTB PC Hash & BTB PC and Global History Hash" << endl;
  *out << "Ratio of mispredictions & "
       << ((double)mispredBTB[0]) / totalpredBTB[0] << " & "
       << ((double)mispredBTB[1]) / totalpredBTB[1] << endl;
  *out << "Ratio of BTB misses & "
       << ((double)missesBTB[0]) / totalpredBTB[0] << " & "
       << ((double)missesBTB[1]) / totalpredBTB[1] << endl;
  *out << "===============================================" << endl;
  exit(0);
}

VOID Fini(INT32 code, VOID *v) { Exit(); }

int main(int argc, char *argv[]) {
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  string fileName = KnobOutputFile.Value();
  string ffCountString = KnobFastForwardCount.Value();
  fast_forward_count = atoll(ffCountString.c_str());
  cout << "Fast forward count: " << fast_forward_count << endl;

  if (!fileName.empty()) {
    out = new std::ofstream(fileName.c_str());
  }

  if (KnobCount) {
    INS_AddInstrumentFunction(Instruction, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);
  }

  cerr << "===============================================" << endl;
  cerr << "This application is instrumented by MyPinTool" << endl;
  if (!KnobOutputFile.Value().empty()) {
    cerr << "See file " << KnobOutputFile.Value()
         << " for analysis results" << endl;
  }
  cerr << "===============================================" << endl;

  PIN_StartProgram();

  return 0;
}
