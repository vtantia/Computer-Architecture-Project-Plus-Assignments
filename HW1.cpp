#include "pin.H"
#include "xed-category-enum.h"
#include <fstream>
#include <iostream>
#define SIZE ((1 << 24) + 1)

void Exit();

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 bblCount = 0; // number of dynamically executed basic blocks

UINT64 fast_forward_count = 30;
UINT64 run_inst_count = 100;
// UINT64 checkedIns = 0;

UINT64 insCountAnalyzed = 0;
UINT64 totalIns = 0;
UINT64 cntInsPrinted = 0;

UINT64 catCounts[100] = {0};
UINT64 cntDirect = 0;
UINT64 cntLoad = 0;
UINT64 cntStore = 0;

std::ostream *out = &cerr;

static UINT64 ArrayIns[SIZE] = {0};
static UINT64 ArrayDat[SIZE] = {0};

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

ADDRINT Terminate(void) {
  return (insCount >= fast_forward_count + run_inst_count);
}

// Analysis routine to check fast-forward condition
ADDRINT FastForward(void) {
  return (insCount >= fast_forward_count && insCount);
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID Analysis(UINT32 category, UINT32 isDirect, UINT32 size, ADDRINT addr,
              BOOL pred) {
  if (pred) {
    insCountAnalyzed++;
    catCounts[category]++;
  }
  cntDirect += (category == XED_CATEGORY_CALL) && isDirect;
  ArrayIns[(addr / 32)] =
      MAX((addr + size - 1) / 32, ArrayIns[(addr / 32)]);
}

VOID CountBbl(UINT32 numInstInBbl) {
  bblCount++;
  insCount += numInstInBbl;
}

VOID RecordMemLoadStore(UINT32 isRead, UINT32 isWrite, UINT32 size,
                        ADDRINT addr, BOOL isMem) {
  cntLoad += (isRead * size + 3) / 4;
  cntStore += (isWrite * size + 3) / 4;
  // cout << addr << " " << size << endl;
  ArrayDat[(addr / 32) * isMem] =
      MAX((addr + size - 1) / 32, isMem * ArrayDat[(addr / 32)]);
}

VOID CountIns(UINT32 numInstInBbl) { insCount++; }

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Instruction(INS ins, VOID *v) {
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_END);

  // If fast forward portion is over, analyze
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Analysis, IARG_UINT32,
                     INS_Category(ins), IARG_UINT32, INS_IsDirectCall(ins),
                     IARG_UINT32, INS_Size(ins), IARG_ADDRINT,
                     INS_Address(ins), IARG_BOOL, INS_IsPredicated(ins),
                     IARG_END);

  UINT32 memOperands = INS_MemoryOperandCount(ins);
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    ADDRINT addr = (INS_OperandMemoryBaseReg(ins, memOp) +
                    INS_OperandMemoryDisplacement(ins, memOp) +
                    INS_OperandMemoryIndexReg(ins, memOp) *
                        INS_OperandMemoryScale(ins, memOp)) /
                   8;
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)RecordMemLoadStore, IARG_UINT32,
        INS_MemoryOperandIsRead(ins, memOp), IARG_UINT32,
        INS_MemoryOperandIsWritten(ins, memOp), IARG_UINT32,
        INS_MemoryOperandSize(ins, memOp), IARG_ADDRINT, addr, IARG_BOOL,
        INS_OperandIsMemory(ins, memOp), IARG_END);
  }

  // If termination region, then exit
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Exit, IARG_END);
}

void printStats(string str, UINT64 cnt) {
  cntInsPrinted += cnt;

  *out << str << " & " << cnt << " & " << ((double)cnt) / totalIns * 100
       << endl;
}

void outputInstructionCounts() {
  printStats("Load", cntLoad);
  printStats("Store", cntStore);
  printStats("NOP", catCounts[XED_CATEGORY_NOP]);
  printStats("Direct call", cntDirect);
  printStats("Indirect call", catCounts[XED_CATEGORY_CALL] - cntDirect);
  printStats("Return", catCounts[XED_CATEGORY_RET]);
  printStats("Unconditional branche", catCounts[XED_CATEGORY_UNCOND_BR]);
  printStats("Conditional branche", catCounts[XED_CATEGORY_COND_BR]);
  printStats("Logical operation", catCounts[XED_CATEGORY_LOGICAL]);
  printStats("Rotate and shift", catCounts[XED_CATEGORY_ROTATE] +
                                     catCounts[XED_CATEGORY_SHIFT]);
  printStats("Flag operation", catCounts[XED_CATEGORY_FLAGOP]);
  printStats("Vector instruction", catCounts[XED_CATEGORY_AVX] +
                                       catCounts[XED_CATEGORY_AVX2] +
                                       catCounts[XED_CATEGORY_AVX2GATHER]);
  printStats("Conditional move", catCounts[XED_CATEGORY_CMOV]);
  printStats("MMX and SSE instruction",
             catCounts[XED_CATEGORY_MMX] + catCounts[XED_CATEGORY_SSE]);
  printStats("System call", catCounts[XED_CATEGORY_SYSCALL]);
  printStats("Floating-point", catCounts[XED_CATEGORY_X87_ALU]);

  UINT64 cntRest = totalIns - cntInsPrinted;
  *out << "Rest"
       << "& " << cntRest << " & " << ((double)cntRest) / totalIns * 100
       << endl;
}

UINT64 findChunks(UINT64 *arr) {
  UINT64 i = 0;
  UINT64 cnt = 0;
  UINT64 ptr = 0;
  for (i = 0; i < SIZE; i++) {
    if (arr[i] != 0) {
      ptr = MAX(ptr, arr[i]);
    }
    if (i <= ptr) {
      cnt += 1;
    }
    i++;
  }
  return cnt;
}

void Exit() {
  totalIns = insCountAnalyzed + cntLoad + cntStore;
  *out << "Fast forward count: " << fast_forward_count << endl;
  *out << "===============================================" << endl;
  *out << "MyPinTool analysis results: " << endl;
  *out << "Number of instructions: " << insCount << endl;
  //*out << "Checked   instructions: " << checkedIns << endl;
  *out << "===============================================" << endl;

  outputInstructionCounts();

  UINT64 totalCycles = insCountAnalyzed * 1 + (cntLoad + cntStore) * 50;
  *out << "CPI: " << ((double)totalCycles) / totalIns << endl;

  *out << endl;
  *out << endl;
  for (int i = 0; i < 76; i++) {
    *out << CATEGORY_StringShort(i) << ": " << catCounts[i] << endl;
  }

  *out << endl;
  *out << endl;
  *out << "Instruction footprint: (multiples of 32): "
       << findChunks(ArrayIns) << endl;
  *out << "Data footprint: (multiples of 32): " << findChunks(ArrayDat)
       << endl;

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
