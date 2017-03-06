#include "pin.H"
#include "xed-category-enum.h"
#include <fstream>
#include <iostream>

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

UINT64 fast_forward_count = 30;
UINT64 run_inst_count = 1000000000;

UINT32 mispredFNBT = 0;
//
UINT32 mispredBimod = 0;
UINT32 bimod[512] = {0};
//
UINT32 sag_bht[1024] = {0};
UINT32 sag_pht[512] = {0};
UINT32 mispredSag = 0;
//
UINT32 gag_bht = 0;
UINT32 gag_pht[512] = {0};
UINT32 mispredGag = 0;
//
UINT32 gshare_bht = 0;
UINT32 gshare_pht[512] = {0};
UINT32 mispredGshare = 0;
//
UINT32 mispredHyb1 = 0;
UINT32 hyb1_sag_bht[1024] = {0};
UINT32 hyb1_sag_pht[512] = {0};
UINT32 hyb1_gag_bht = 0;
UINT32 hyb1_gag_pht[512] = {0};
UINT32 hyb1_meta[512] = {0};
//
UINT32 mispredHyb2maj = 0;
//
UINT32 mispredHyb2meta = 0;
UINT32 hyb2_s_g[512] = {0};
UINT32 hyb2_s_gsh[512] = {0};
UINT32 hyb2_g_gsh[512] = {0};

#define NUM_BTB 128
UINT32 mispredBTB[2] = {0};
UINT32 lastTimeBTB[2] = {0};
UINT32 btbHist = 0;
BtbEntry btb[2][NUM_BTB][4];

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

VOID updateCount(UINT32 *toChange, BOOL isTaken, UINT32 limit) {
  if (*toChange != limit && isTaken)
    (*toChange)++;
  else if (*toChange != 0 && !isTaken)
    (*toChange)--;
}

VOID BranchPredFNBT(ADDRINT insAddr, ADDRINT bTargetAddr, BOOL isTaken) {
  mispredFNBT += ((bTargetAddr - insAddr) > 0) ? isTaken : !isTaken;

  mispredBimod += (bimod[(insAddr) % 512] < 2) ? isTaken : !isTaken;
  updateCount(&bimod[(insAddr) % 512], isTaken, 3);

  UINT32 hist_sag = sag_bht[insAddr % 1024];
  UINT32 sag_pred = (sag_pht[hist_sag] < 4) ? 0 : 1;
  mispredSag += (sag_pht[hist_sag] < 4) ? isTaken : !isTaken;
  updateCount(&sag_pht[hist_sag], isTaken, 7);
  sag_bht[insAddr % 1024] =
      ((sag_bht[insAddr % 1024] << 1) + isTaken) & 0x1ff;

  UINT32 gag_pred = (gag_pht[gag_bht] < 4) ? 0 : 1;
  mispredGag += (gag_pht[gag_bht] < 4) ? isTaken : !isTaken;
  updateCount(&gag_pht[gag_bht], isTaken, 7);
  gag_bht = ((gag_bht << 1) + isTaken) & 0x1ff;

  UINT32 index = gshare_bht ^ (insAddr & 0x1ff);
  UINT32 gshare_pred = (gshare_pht[index] < 4);
  mispredGshare += (gshare_pht[index] < 4) ? isTaken : !isTaken;
  updateCount(&gshare_pht[index], isTaken, 7);
  gshare_bht = ((gshare_bht << 1) + isTaken) & 0x1ff;

  UINT32 needUpdate = ((!gag_pred) * sag_pred) + (gag_pred * (!sag_pred));
  mispredHyb1 += (hyb1_meta[gag_bht] < 2) ? (isTaken != sag_pred)
                                          : (isTaken != gag_pred);
  if (needUpdate) {
    // GAg is assumed on high end of counter
    UINT32 gagCorrect = (gag_pred == isTaken);
    updateCount(&hyb1_meta[gag_bht], gagCorrect, 3);
  }

  UINT32 majVote = (gshare_pred + gag_pred + sag_pred) > 1;
  mispredHyb2maj += (majVote != isTaken);

  UINT32 metaVote = 0;
  if (hyb2_s_g[gag_bht] < 2 && hyb2_s_gsh[gag_bht] < 2) {
    metaVote = sag_pred;
  } else if (hyb2_g_gsh[gag_bht] > 1 && hyb2_s_gsh[gag_bht] > 1) {
    metaVote = gshare_pred;
  } else if (hyb2_g_gsh[gag_bht] < 2 && hyb2_s_g[gag_bht] > 1) {
    metaVote = gag_pred;
  } else {
    metaVote = (gshare_pred + gag_pred + sag_pred) > 1;
  }
  mispredHyb2meta += (metaVote != isTaken);
  UINT32 update_s_g = ((!gag_pred) * sag_pred) + (gag_pred * (!sag_pred));
  UINT32 update_s_gsh =
      ((!gshare_pred) * sag_pred) + (gshare_pred * (!sag_pred));
  UINT32 update_g_gsh =
      ((!gag_pred) * gshare_pred) + (gag_pred * (!gshare_pred));
  if (update_s_g) // Second is higher
    updateCount(&hyb2_s_g[gag_bht], (gag_pred == isTaken), 3);
  if (update_s_gsh) // Second is higher
    updateCount(&hyb2_s_gsh[gag_bht], (gshare_pred == isTaken), 3);
  if (update_g_gsh) // Second is higher
    updateCount(&hyb2_g_gsh[gag_bht], (gshare_pred == isTaken), 3);
}

VOID UpdateCondBrHist(BOOL isTaken) {
  btbHist = (btbHist << 1) % NUM_BTB;
  btbHist += isTaken;
}

VOID IndirectPred(ADDRINT insAddr, ADDRINT nextInsAddr, ADDRINT targetAddr,
                  UINT32 cacheIndex, UINT32 btbIndex) {
  UINT32 invalidIndex = 4, lruIndex = 4;
  BOOL found = false;

  for (UINT32 i = 0; i < 4; i++) {
    BtbEntry &b = btb[btbIndex][cacheIndex][i];

    if (!b.valid) {
      invalidIndex = i;
    } else if (b.tag == insAddr) {
      mispredBTB[btbIndex] += !(targetAddr == b.target);
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
  if (!found && targetAddr != nextInsAddr) {
    mispredBTB[btbIndex] += 1;

    btb[btbIndex][cacheIndex][toUpdate] = {true, insAddr, targetAddr,
                                           lastTimeBTB[btbIndex]++};
    /*BtbEntry &b =*/ // b.valid = true;
    // b.tag = insAddr;
    // b.target = targetAddr;
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
  if (INS_IsDirectBranch(ins)) {
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)BranchPredFNBT, IARG_ADDRINT,
        INS_Address(ins),        // instruction address
        IARG_BRANCH_TARGET_ADDR, // branch target address
        IARG_BRANCH_TAKEN,       // if branch is taken
        IARG_END);
  }

  if (INS_Category(ins) == XED_CATEGORY_COND_BR) {
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)UpdateCondBrHist,
        IARG_BRANCH_TAKEN, // branch target address
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

void Exit() {
  *out << "Fast forward count: " << fast_forward_count << endl;
  *out << "===============================================" << endl;
  *out << "MyPinTool analysis results: " << endl;
  *out << "Number of instructions: " << insCount << endl;
  *out << "===============================================" << endl;
  *out << "Mispredictions in FNBT : " << mispredFNBT << endl;
  *out << "Mispredictions in bimodal : " << mispredBimod << endl;
  *out << "Mispredictions in SAg : " << mispredSag << endl;
  *out << "Mispredictions in GAg : " << mispredGag << endl;
  *out << "Mispredictions in Gshare : " << mispredGshare << endl;
  *out << "Mispredictions in Hybrid1 : " << mispredHyb1 << endl;
  *out << "Mispredictions in Hybrid2-1 : " << mispredHyb2maj << endl;
  *out << "Mispredictions in Hybrid2-2 : " << mispredHyb2meta << endl;
  *out << "Mispredictions in BTB PC : " << mispredBTB[0] << endl;
  *out << "Mispredictions in BTB PC and global history : " << mispredBTB[1]
       << endl;
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
