#include "pin.H"
#include "xed-category-enum.h"
#include <fstream>
#include <iostream>
#define SIZE ((1 << 27) + 1)
#define SIZED 200

void Exit();

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions

UINT64 fast_forward_count = 30;
UINT64 run_inst_count = 1000000000;
// UINT64 checkedIns = 0;

UINT64 insCountAnalyzed = 0;
UINT64 totalIns = 0;
UINT64 cntInsPrinted = 0;

UINT64 catCounts[100] = {0};
UINT64 cntDirect = 0;
// UINT64 cntIndirect = 0;
UINT64 cntLoad = 0;
UINT64 cntStore = 0;

// UINT64 numMemReadOp = 0;
// UINT64 numMemWriteOp = 0;

UINT64 maxBytes = 0;
UINT64 totalBytes = 0;
UINT64 avgBytes = 0;
UINT64 numMemIns = 0;

INT32 maxImmed = INT32_MIN;
INT32 minImmed = INT32_MAX;
ADDRDELTA maxDispl = INT32_MIN;
ADDRDELTA minDispl = INT32_MAX; // Only for ia32 arch target

UINT64 insLens[SIZED];
UINT64 numOps[SIZED];
UINT64 numReadOps[SIZED];
UINT64 numWriteOps[SIZED];
UINT64 numMemOps[SIZED];
UINT64 numMemReadOps[SIZED];
UINT64 numMemWriteOps[SIZED];

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
ADDRINT FastForward(void) { return (insCount >= fast_forward_count); }

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

VOID Analysis(UINT32 category, BOOL isDirect) {
  insCountAnalyzed++;
  catCounts[category]++;
  cntDirect += ((category == XED_CATEGORY_CALL) && isDirect);
}

VOID RecordInstrFootprint(ADDRINT arrayIndex, UINT32 numChunks,
                          UINT32 size, UINT32 numOp, UINT32 numReadOp,
                          UINT32 numWriteOp) {
  ArrayIns[arrayIndex] = numChunks;

  insLens[size]++;
  numOps[numOp]++;
  numReadOps[numReadOp]++;
  numWriteOps[numWriteOp]++;
}

VOID RecordMemLoadStore(BOOL isRead, BOOL isWrite, UINT32 size,
                        ADDRINT addr, ADDRDELTA displ) {
  cntLoad += (isRead * size + 3) / 4;
  cntStore += (isWrite * size + 3) / 4;
  ArrayDat[(addr / 32) * (isRead || isWrite)] =
      MAX((addr + size - 1) / 32, ArrayIns[(addr / 32)]);

  maxDispl = MAX(displ, maxDispl);
  minDispl = MIN(displ, minDispl);
}

VOID UpdateImmediateMaxMin(INT32 sizeImmed) {
  maxImmed = MAX(maxImmed, sizeImmed);
  minImmed = MIN(minImmed, sizeImmed);
}

VOID FindMemoryUsageOfInstr(USIZE readSize, UINT32 writeSize,
                            UINT32 numMemReadOp, UINT32 numMemWriteOp) {
  numMemOps[numMemReadOp + numMemWriteOp]++;
  numMemReadOps[numMemReadOp]++;
  numMemWriteOps[numMemWriteOp]++;

  UINT32 bytes = (readSize + writeSize);
  maxBytes = MAX(bytes, maxBytes);
  totalBytes += bytes;
  numMemIns += (bytes > 0);
}

// VOID CountIns(UINT32 numInst) { insCount++; }

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Instruction(INS ins, VOID *v) {
  // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_END);

  ADDRINT addr = INS_Address(ins);
  UINT32 size = INS_Size(ins);
  UINT32 chunkSize = MAX((addr + size - 1) / 32, ArrayIns[(addr / 32)]);
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // If fast forward portion is over, analyze
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
  INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Analysis,
                               IARG_UINT32, INS_Category(ins), IARG_BOOL,
                               INS_IsDirectCall(ins), IARG_END);

  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
  INS_InsertThenCall(
      ins, IPOINT_BEFORE, (AFUNPTR)RecordInstrFootprint, IARG_ADDRINT,
      (addr / 32), IARG_UINT32, chunkSize, IARG_UINT32, size, IARG_UINT32,
      INS_MemoryOperandCount(ins), IARG_UINT32, INS_MaxNumRRegs(ins),
      IARG_UINT32, INS_MaxNumWRegs(ins), IARG_END);

  UINT32 numMemReadOp = 0, numMemWriteOp = 0;
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {

    numMemReadOp += INS_MemoryOperandIsRead(ins, memOp);
    numMemWriteOp += INS_MemoryOperandIsWritten(ins, memOp);

    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR)RecordMemLoadStore, IARG_BOOL,
        INS_MemoryOperandIsRead(ins, memOp), IARG_BOOL,
        INS_MemoryOperandIsWritten(ins, memOp), IARG_UINT32,
        INS_MemoryOperandSize(ins, memOp), IARG_MEMORYOP_EA, memOp,
        IARG_ADDRINT, INS_OperandMemoryDisplacement(ins, memOp), IARG_END);
  }

  UINT32 totalOperands = INS_OperandCount(ins);
  for (UINT32 opNum = 0; opNum < totalOperands; opNum++) {
    // This has to be done for all instructions, not just predicated
    // Thus, a new IfThen call
    if (INS_OperandIsImmediate(ins, opNum)) {
      INT32 kk = INS_OperandImmediate(ins, opNum);
      INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
      INS_InsertThenCall(ins, IPOINT_BEFORE,
                         (AFUNPTR)UpdateImmediateMaxMin, IARG_ADDRINT, kk,
                         IARG_END);
    }
  }

  UINT32 mrs = 0, mws = 0;
  if (INS_IsMemoryRead(ins)) {
    mrs = INS_MemoryReadSize(ins);
  }
  if (INS_IsMemoryWrite(ins)) {
    mws = INS_MemoryWriteSize(ins);
  }

  // No need to pass IsMemoryRead/Write. It was only needed to zero out
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
  INS_InsertThenPredicatedCall(
      ins, IPOINT_BEFORE, (AFUNPTR)FindMemoryUsageOfInstr, IARG_UINT32,
      mrs, IARG_UINT32, mws, IARG_UINT32, numMemReadOp, IARG_UINT32,
      numMemWriteOp, IARG_END);

  // If termination region, then exit
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
  //*out << "Test: " << cntDirect << "+" << cntIndirect << " should be "
  //<< catCounts[XED_CATEGORY_CALL] << " and is "
  //<< cntDirect + cntIndirect << endl;
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

void printD(string str, UINT64 *arr) {
  *out << str << endl;
  for (int i = 0; i < SIZED; i++) {
    if (arr[i] != 0) {
      *out << i << ": " << arr[i] << endl;
    }
  }
  *out << endl;
}

void outputPartD() {
  printD("1. Distribution of instruction length: ", insLens);
  printD("2. Distribution of the number of operands in an instruction: ",
         numOps);
  printD("3. Distribution of the number of register read operands in an "
         "instruction: ",
         numReadOps);
  printD("4. Distribution of the number of register write operands in an "
         "instruction: ",
         numWriteOps);
  printD("5. Distribution of the number of memory operands in an "
         "instruction: ",
         numMemOps);
  printD("6. Distribution of the number of memory read operands in an "
         "instruction: ",
         numMemReadOps);
  printD("7. Distribution of the number of memory write operands in an "
         "instruction: ",
         numMemWriteOps);
  *out << "8. Max bytes touched: " << maxBytes
       << ", average: " << totalBytes / numMemIns << endl;
  *out << "9. Max immediate field: " << maxImmed
       << ", min immediate field: " << minImmed << endl;
  *out << "10. Max displacement field: " << maxDispl
       << ", min displacement field: " << minDispl << endl;
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
  *out << "Instruction footprint: (in multiples of 32 bytes): "
       << findChunks(ArrayIns) << endl;
  *out << "Data footprint: (in multiples of 32 bytes): "
       << findChunks(ArrayDat) << endl;

  outputPartD();

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
