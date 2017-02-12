#include "pin.H"
#include "xed-category-enum.h"
#include <iostream>
#include <fstream>

void Exit();

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0;        //number of dynamically executed instructions
UINT64 bblCount = 0;        //number of dynamically executed basic blocks

UINT64 fast_forward_count = 207000000000;
UINT64 run_inst_count = 1000000000;
UINT64 checkedIns = 0;

UINT64 catCounts[100] = {0};

std::ostream * out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "", "specify file name for MyPinTool output");

KNOB<BOOL>   KnobCount(KNOB_MODE_WRITEONCE,  "pintool",
    "count", "1", "count instructions, basic blocks  in the application");


/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage() {
    cerr << "This tool prints out the number of dynamically executed " << endl <<
            "instructions, basic blocks  in the application." << endl << endl;
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

VOID Analysis(UINT32 category) {
    catCounts[category]++;
}

VOID CountBbl(UINT32 numInstInBbl) {
    bblCount++;
    insCount += numInstInBbl;
}

VOID CountIns(UINT32 numInstInBbl) {
    insCount ++;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Instruction(INS ins, VOID *v) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIns, IARG_END);

    // If fast forward portion is over, analyze
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)Analysis,
            IARG_UINT32, INS_Category(ins), IARG_END);

    // If termination region, then exit
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Exit, IARG_END);
}

void Exit() {
    *out <<  "===============================================" << endl;
    *out <<  "MyPinTool analysis results: " << endl;
    *out <<  "Number of instructions: " << insCount  << endl;
    *out <<  "Checked   instructions: " << checkedIns  << endl;
    *out <<  "===============================================" << endl;

    for (int i=0; i<76; i++) {
        *out << CATEGORY_StringShort(i) << ": " << catCounts[i] << endl;
    }
    exit(0);
}

VOID Fini(INT32 code, VOID *v) {
    Exit();
}

int main(int argc, char *argv[]) {
    if( PIN_Init(argc,argv) ) {
        return Usage();
    }

    string fileName = KnobOutputFile.Value();

    if (!fileName.empty()) { out = new std::ofstream(fileName.c_str());}

    if (KnobCount) {
        INS_AddInstrumentFunction(Instruction, 0);
        PIN_AddFiniFunction(Fini, 0);
    }

    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty()) {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr <<  "===============================================" << endl;

    PIN_StartProgram();

    return 0;
}
