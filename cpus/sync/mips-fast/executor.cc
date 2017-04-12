#include "executor.h"
#include "common.h"

Exe::Exe(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->ex_mem, 0, sizeof(pipeline->ex_mem));
    pipeline->ex_mem._kill = TRUE;
}

Exe::~Exe(void) {
}

ID_EX ex_pipe;

void Exe::MainLoop(void) {
    unsigned int ins;
    Bool isSyscall, isIllegalOp;

    while (1) {
        if (!pipeline->id_ex._kill) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;

            if (pipeline->id_ex._skipExec) {
                AWAIT_P_PHI1;  // @negedge
                pipeline->id_ex._skipExec = false;

                pipeline->ex_mem._kill = true;
                continue;
            }

            ex_pipe = pipeline->id_ex;
            ins = pipeline->id_ex._ins;

            isSyscall = ex_pipe._isSyscall;
            isIllegalOp = ex_pipe._isIllegalOp;

            if (!isSyscall && !isIllegalOp) {
                // TODO: Think about _lo, _hi
                pipeline->getBypassValue(
                    ex_pipe._rs,
                    &(ex_pipe._decodedSRC1));

                pipeline->getBypassValue(
                    ex_pipe._rt,
                    &(ex_pipe._decodedSRC2));

                // Run pipe
                ex_pipe._opControl(_mc, ins);

                if (!ex_pipe._memControl && ex_pipe._writeREG) {
                    pipeline->ex_mem.bypass.storeValueFromEx(
                        ex_pipe._opResultLo, ex_pipe._decodedDST);
                } else {
                    pipeline->ex_mem.bypass.invalidateEx();
                }

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
            cout << "_mc->_bd is " << _mc->_bd << endl;
            cout << "pipeline->_bd is " << pipeline->id_ex._bd << endl;
            cout << "pipeline->_btgt is " << pipeline->id_ex._btgt << endl;
            if (!isIllegalOp && !isSyscall) {
                if(pipeline->id_ex._bd) {
                    if (pipeline->id_ex._bd && _mc->_btaken) {
                        _mc->_pc = pipeline->id_ex._btgt;
                    } else {
                        cout << "Incrementing _pc because SWAG " << _mc->_btaken << endl;
                        _mc->_pc = _mc->_pc + 4;
                    }
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

            pipeline->ex_mem._kill = FALSE;
        } else {
            AWAIT_P_PHI0;  // @posedge
            AWAIT_P_PHI1;  // @negedge
            pipeline->ex_mem._kill = TRUE;
            // PAUSE(1);
        }
    }
}
