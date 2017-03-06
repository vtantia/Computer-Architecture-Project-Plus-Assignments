#include "pin.H"
#include "xed-category-enum.h"
#include <fstream>
#include <iostream>

void Exit();

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions

UINT64 fast_forward_count = 30;
UINT64 run_inst_count = 1000000000;

UINT32 mispredFNBT = 0;
UINT32 mispredBimod = 0;
UINT32 bimod[512] = {0};

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
  UINT32 index = (insAddr >> 2) % 512;

  mispredFNBT += ((bTargetAddr - insAddr) > 0) ? isTaken : !isTaken;

  mispredBimod += (bimod[index] < 2) ? isTaken : !isTaken;
  updateCount(&bimod[index], isTaken, 3);
}

ADDRINT Terminate(void) {
  return (insCount >= fast_forward_count + run_inst_count);
}

// Analysis routine to check fast-forward condition
ADDRINT FastForward(void) { return (insCount >= fast_forward_count); }

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

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
