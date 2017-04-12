#include "wb.h"
#include "common.h"

Writeback::Writeback(Mipc *mc) {
    _mc = mc;
}

Writeback::~Writeback(void) {
}

void Writeback::MainLoop(void) {
    unsigned int ins;
    Bool writeReg;
    Bool writeFReg;
    Bool loWPort;
    Bool hiWPort;
    Bool isSyscall;
    Bool isIllegalOp;
    unsigned decodedDST;
    unsigned opResultLo, opResultHi;

    while (1) {
        // Sample the important signals
        if (!pipeline->mem_wb._kill) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;
            writeReg = pipeline->mem_wb._writeREG;
            writeFReg = pipeline->mem_wb._writeFREG;
            loWPort = pipeline->mem_wb._loWPort;
            hiWPort = pipeline->mem_wb._hiWPort;
            decodedDST = pipeline->mem_wb._decodedDST;
            opResultLo = pipeline->mem_wb._opResultLo;
            opResultHi = pipeline->mem_wb._opResultHi;
            isSyscall = pipeline->mem_wb._isSyscall;
            isIllegalOp = pipeline->mem_wb._isIllegalOp;

            ins = pipeline->mem_wb._ins;

            DDBG;
            AWAIT_P_PHI1;  // @negedge
            DDBG;
            if (isSyscall) {
#ifdef MIPC_DEBUG
                fprintf(debugLog,
                        "<%llu> SYSCALL! Trapping to emulation layer at PC %#x\n",
                        SIM_TIME, _mc->_pc);
#endif
                _mc->_opControl(_mc, ins);
                // MAGIC HERE. Please check
                //_mc->_pc += 4;
            } else if (isIllegalOp) {
                printf("Illegal ins %#x at PC %#x. Terminating simulation!\n", ins,
                       _mc->_pc);
#ifdef MIPC_DEBUG
                fclose(debugLog);
#endif
                printf("Register state on termination:\n\n");
                _mc->dumpregs();
                exit(0);
            } else {
                if (writeReg) {
                    _mc->_gpr[decodedDST] = opResultLo;
#ifdef MIPC_DEBUG
                    fprintf(debugLog, "<%llu> Writing to reg %u, value: %#x\n", SIM_TIME,
                            decodedDST, opResultLo);
#endif
                } else if (writeFReg) {
                    _mc->_fpr[(decodedDST) >> 1].l[FP_TWIDDLE ^ ((decodedDST)&1)] =
                        opResultLo;
#ifdef MIPC_DEBUG
                    fprintf(debugLog, "<%llu> Writing to freg %u, value: %#x\n", SIM_TIME,
                            decodedDST >> 1, opResultLo);
#endif
                } else if (loWPort || hiWPort) {
                    if (loWPort) {
                        _mc->_lo = opResultLo;
#ifdef MIPC_DEBUG
                        fprintf(debugLog, "<%llu> Writing to Lo, value: %#x\n", SIM_TIME,
                                opResultLo);
#endif
                    }
                    if (hiWPort) {
                        _mc->_hi = opResultHi;
#ifdef MIPC_DEBUG
                        fprintf(debugLog, "<%llu> Writing to Hi, value: %#x\n", SIM_TIME,
                                opResultHi);
#endif
                    }
                }
            }

            if (pipeline->mem_wb._isSyscall) {
                pipeline->if_id._fetch_kill = FALSE;
            }

            _mc->_gpr[0] = 0;
            //_mc->_memValid = FALSE;
            //_mc->_insDone = TRUE;
        } else {
            PAUSE(1);
        }
    }
}
