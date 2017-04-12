#include "common.h"

Bypass::Bypass() {
    valid1 = false;
    valid2 = false;
}

void Common::copyFromMc(Mipc *mc) {
    _rs = mc->_rs;
    _rt = mc->_rt;
    _ins = mc->_ins;
    _decodedSRC1 = mc->_decodedSRC1;
    _decodedSRC2 = mc->_decodedSRC2;
    _decodedDST = mc->_decodedDST;
    _subregOperand = mc->_subregOperand;
    _MAR = mc->_MAR;
    _opResultHi = mc->_opResultHi;
    _opResultLo = mc->_opResultLo;
    _memControl = mc->_memControl;
    _writeREG = mc->_writeREG;
    _writeFREG = mc->_writeFREG;
    _branchOffset = mc->_branchOffset;
    _hiWPort = mc->_hiWPort;
    _loWPort = mc->_loWPort;
    _decodedShiftAmt = mc->_decodedShiftAmt;

    _hi = mc->_hi;
    _lo = mc->_lo;
    _lastbd = mc->_lastbd;
    _boot = mc->_boot;

    _btaken = mc->_btaken;
    _bd = mc->_bd;
    _btgt = mc->_btgt;

    _isSyscall = mc->_isSyscall;
    _isIllegalOp = mc->_isIllegalOp;

    _opControl = mc->_opControl;
    _memOp = mc->_memOp;
}

void Common::copyFromPipe(Common *pipe) {
    _rs = pipe->_rs;
    _rt = pipe->_rt;

    _ins = pipe->_ins;
    _decodedSRC1 = pipe->_decodedSRC1;
    _decodedSRC2 = pipe->_decodedSRC2;
    _decodedDST = pipe->_decodedDST;
    _subregOperand = pipe->_subregOperand;
    _MAR = pipe->_MAR;
    _opResultHi = pipe->_opResultHi;
    _opResultLo = pipe->_opResultLo;
    _memControl = pipe->_memControl;
    _writeREG = pipe->_writeREG;
    _writeFREG = pipe->_writeFREG;
    _branchOffset = pipe->_branchOffset;
    _hiWPort = pipe->_hiWPort;
    _loWPort = pipe->_loWPort;
    _decodedShiftAmt = pipe->_decodedShiftAmt;

    _hi = pipe->_hi;
    _lo = pipe->_lo;
    _lastbd = pipe->_lastbd;
    _boot = pipe->_boot;

    _btaken = pipe->_btaken;
    _bd = pipe->_bd;
    _btgt = pipe->_btgt;

    _isSyscall = pipe->_isSyscall;
    _isIllegalOp = pipe->_isIllegalOp;

    _opControl = pipe->_opControl;
    _memOp = pipe->_memOp;
}

void Pipereg::getBypassValue(int kaunsaReg, int *kidharStore) {
    bool didIGetFromMemBypass =
        pipeline->mem_wb.bypass.canIGetNewValueOfReg(
            kaunsaReg, kidharStore);
    if (!didIGetFromMemBypass) {
        pipeline->ex_mem.bypass.canIGetNewValueOfReg(
            kaunsaReg, kidharStore);
    }
}
