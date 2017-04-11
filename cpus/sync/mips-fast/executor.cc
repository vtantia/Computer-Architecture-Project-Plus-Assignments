#include "executor.h"
#include "common.h"

Exe::Exe(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->ex_mem, 0, sizeof(pipeline->ex_mem));
    pipeline->ex_mem._memControl = FALSE;
}

Exe::~Exe(void) {
}

ID_EX ex_pipe;

void Exe::MainLoop(void) {
    unsigned int ins;
    Bool isSyscall, isIllegalOp;

    while (1) {
        if ((ins = pipeline->id_ex._ins) != 0) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;
            ex_pipe = pipeline->id_ex;

            isSyscall = ex_pipe._isSyscall;
            isIllegalOp = ex_pipe._isIllegalOp;

            if (!isSyscall && !isIllegalOp) {
                ex_pipe._opControl(_mc, ins);
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Executed ins %#x\n", SIM_TIME, ins);
#endif
            } else if (isSyscall) {
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Deferring execution of syscall ins %#x\n",
                        SIM_TIME, ins);
#endif
            } else {
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Illegal ins %#x in execution stage at PC %#x\n",
                        SIM_TIME, ins, _mc->_pc);
#endif
            }
            //_mc->_decodeValid = FALSE;
            //_mc->_execValid = TRUE;

            if (!isIllegalOp && !isSyscall) {
                if (_mc->_lastbd && _mc->_btaken) {
                    _mc->_pc = _mc->_btgt;
                //} else {
                    //_mc->_pc = _mc->_pc + 4;
                }
                _mc->_lastbd = _mc->_bd;
            }

            DDBG;
            AWAIT_P_PHI1;  // @negedge
            DDBG;
            pipeline->ex_mem.copyFromPipe(&ex_pipe);
            pipeline->ex_mem._MAR = _mc->_MAR;
            pipeline->ex_mem._opResultLo = _mc->_opResultLo;
            pipeline->ex_mem._opResultHi = _mc->_opResultHi;
            pipeline->ex_mem._btaken = _mc->_btaken;
        } else {
            PAUSE(1);
        }
    }
}
