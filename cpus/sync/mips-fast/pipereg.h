#ifndef _PIPEREG_H
#define _PIPEREG_H

#include "mips.h"

class Common {
public:
    /* processor state */
    unsigned int _ins;  // instruction register

    signed int _decodedSRC1, _decodedSRC2;  // Reg fetch output (source values)
    unsigned _decodedDST;                   // Decoder output (dest reg no)
    unsigned _subregOperand;                // Needed for lwl and lwr
    unsigned _MAR;                          // Memory address register
    unsigned _opResultHi, _opResultLo;      // Result of operation
    Bool _memControl;                       // Memory instruction?
    Bool _writeREG, _writeFREG;             // WB control
    signed int _branchOffset;
    Bool _hiWPort, _loWPort;    // WB control
    unsigned _decodedShiftAmt;  // Shift amount

    unsigned int _hi, _lo;  // mult, div destination
    unsigned int _pc;       // Program counter
    unsigned int _lastbd;   // branch delay state
    unsigned int _boot;     // boot code loaded?

    int _btaken;         // taken branch (1 if taken, 0 if fall-through)
    int _bd;             // 1 if the next ins is delay slot
    unsigned int _btgt;  // branch target

    Bool _isSyscall;    // 1 if system call
    Bool _isIllegalOp;  // 1 if illegal opcode

    void (*_opControl)(Mipc *, unsigned);
    void (*_memOp)(Mipc *);

    void copyFromMc(Mipc *);
    void copyFromPipe(Common *);
};

class IF_ID {
public:
    unsigned int _ins;
};

class ID_EX : public Common {
public:
};

class EX_MEM : public Common {
public:
};

class MEM_WB : public Common {
public:
};

class Pipereg {
public:
    Pipereg(){};
    ~Pipereg(){};

    // Instruction Fetch / Decode Pipeline register
    IF_ID if_id;

    // Instruction Decode / Execute Pipeline register
    ID_EX id_ex;

    // Instruction Execute / Memory phase Pipeline register
    EX_MEM ex_mem;

    // Memory phase / Write back Pipeline register
    MEM_WB mem_wb;
};

#endif
