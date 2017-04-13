#ifndef _PIPEREG_H
#define _PIPEREG_H

#include "mips.h"

class Bypass {
public:
    int storedReg1, storedReg2;
    int storedVal1, storedVal2;

    bool valid1, valid2;

    Bypass();

    void storeValueFromEx(int reg, int val) {
        storedReg1 = reg;
        storedVal1 = val;
        valid1 = true;
    }

    void storeValueFromMem(int reg, int val) {
        storedReg2 = reg;
        storedVal2 = val;
        valid2 = true;
    }

    void invalidateEx() {
        valid1 = false;
    }

    void invalidateMem() {
        valid2 = false;
    }

    bool canIGetNewValueOfReg(int reg, int *val) {
        if (storedReg1 == reg && valid1) {
            *val = storedVal1;
            return true;
        }
        if (storedReg2 == reg && valid2) {
            *val = storedVal2;
            return true;
        }
        return false;
    }

    bool canIGetNewValueOfRegUnsigned(int reg, unsigned int *val) {
        if (storedReg1 == reg && valid1) {
            *val = storedVal1;
            return true;
        }
        if (storedReg2 == reg && valid2) {
            *val = storedVal2;
            return true;
        }
        return false;
    }
};

/* class Common { */
/* public: */
/*     /\* processor state *\/ */
/*     unsigned int _ins;  // instruction register */

/*     int _rs, _rt; */

/*     signed int _decodedSRC1, _decodedSRC2;  // Reg fetch output (source values) */
/*     unsigned _decodedDST;                   // Decoder output (dest reg no) */
/*     unsigned _subregOperand;                // Needed for lwl and lwr */
/*     unsigned _MAR;                          // Memory address register */
/*     unsigned _opResultHi, _opResultLo;      // Result of operation */
/*     Bool _memControl;                       // Memory instruction? */
/*     Bool _writeREG, _writeFREG;             // WB control */
/*     signed int _branchOffset; */
/*     Bool _hiWPort, _loWPort;    // WB control */
/*     unsigned _decodedShiftAmt;  // Shift amount */

/*     unsigned int _hi, _lo;  // mult, div destination */
/*     unsigned int _pc;       // Program counter */
/*     unsigned int _boot;     // boot code loaded? */

/*     int _btaken;         // taken branch (1 if taken, 0 if fall-through) */
/*     int _bd;             // 1 if the next ins is delay slot */
/*     unsigned int _btgt;  // branch target */

/*     Bool _isSyscall;    // 1 if system call */
/*     Bool _isIllegalOp;  // 1 if illegal opcode */

/*     Bool _kill;         // Kill signal for pipeline stage */

/*     void (*_opControl)(Mipc *, unsigned); */
/*     void (*_memOp)(Mipc *); */

/*     void copyFromMc(Mipc *); */
/*     void copyFromPipe(Common *); */
/* }; */

class IF_ID {
public:
    unsigned int _ins;
    Bool _kill;         // Kill signal for pipeline stage
    Bool _fetch_kill;   // Kill signal for pipeline stage
    int _was_branch;
    Mipc mc;

    IF_ID(Mipc *mh) {
        mc = *(new Mipc(mh));
    }
};

class ID_EX {
public:
    Bool _kill;         // Kill signal for pipeline stage
    bool _skipExec;     // Have to skip the next EXEC phase
    Mipc mc;

    int src1, src2, subreg;

    ID_EX(Mipc *mh) {
        mc = *(new Mipc(mh));
    }
};

class EX_MEM {
public:
    Bool _kill;         // Kill signal for pipeline stage
    Bypass bypass;
    Mipc mc;
    EX_MEM(Mipc *mh) {
        mc = *(new Mipc(mh));
    }
};

class MEM_WB {
public:
    Bool _kill;         // Kill signal for pipeline stage
    Bypass bypass;
    Mipc mc;
    MEM_WB(Mipc *mh) {
        mc = *(new Mipc(mh));
    }
};

class Pipereg {
public:
    Pipereg(Mipc *mh) : if_id(mh), id_ex(mh), ex_mem(mh), mem_wb(mh) {};
    ~Pipereg(){};

    // Instruction Fetch / Decode Pipeline register
    IF_ID if_id;

    // Instruction Decode / Execute Pipeline register
    ID_EX id_ex;

    // Instruction Execute / Memory phase Pipeline register
    EX_MEM ex_mem;

    // Memory phase / Write back Pipeline register
    MEM_WB mem_wb;

    void getBypassValue(int kaunsaReg, int *kidharStore);
    void getBypassValueUnsigned(int kaunsaReg, unsigned int *kidharStore);
};

#endif
