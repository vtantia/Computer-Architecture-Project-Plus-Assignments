#include "wb.h"
#include "common.h"

Writeback::Writeback(Mipc *mc) {
    _mc = mc;
}

Writeback::~Writeback(void) {
}

void Writeback::MainLoop(void) {
    unsigned int ins;
    while (1) {
        // Sample the important signals
        AWAIT_P_PHI0;
        if (!pipeline->mem_wb._kill) {

            Mipc local_mc = pipeline->mem_wb.mc;

            ins = local_mc._ins;

            int pos = pipeline->mem_wb.mc.position;

            if (local_mc._isSyscall) {
                // for (int kk=0; kk<32; kk++) {
                //     INLOG((inlog, "Register %d is %#x\n", kk, _mc->_gpr[kk]));
                // }
                DBG((debugLog,
                     "<%llu> SYSCALL! Trapping to emulation layer at PC %#x\n",
                     SIM_TIME, _mc->_pc));

                INLOG((inlog, "Executing syscall now\n"));
                local_mc._opControl(&(local_mc), ins);
                // for (int kk=0; kk<32; kk++) {
                //     INLOG((inlog, "Register %d is %#x\n", kk, _mc->_gpr[kk]));
                // }

                _mc->_pc = _mc->_pc + 4;

            } else if (local_mc._isIllegalOp) {

                printf("Illegal ins %#x at PC %#x. Terminating simulation!\n", ins,
                       local_mc._pc);
#ifdef MIPC_DEBUG
                fclose(debugLog);
#endif
                printf("Register state on termination:\n\n");
                _mc->dumpregs();
                exit(0);

            } else {

                if (local_mc._writeREG) {
                    _mc->_gpr[local_mc._decodedDST] = local_mc._opResultLo;

                    DBG((debugLog, "<%llu> Writing to reg %u, value: %#x\n", SIM_TIME,
                         local_mc._decodedDST, local_mc._opResultLo));

                    if (local_mc._opResultLo == -1) {
                        INLOG((inlog, "%3d: DANGER !!!!\n", pos));
                    }

                } else if (local_mc._writeFREG) {
                    _mc->_fpr[(local_mc._decodedDST) >> 1].l[FP_TWIDDLE ^ ((local_mc._decodedDST)&1)] =
                        local_mc._opResultLo;

                    DBG((debugLog, "<%llu> Writing to freg %u, value: %#x\n", SIM_TIME,
                         local_mc._decodedDST >> 1, local_mc._opResultLo));

                } else if (local_mc._loWPort || local_mc._hiWPort) {
                    if (local_mc._loWPort) {
                        _mc->_lo = local_mc._opResultLo;
                        pipeline->id_ex.mc._lo = local_mc._opResultLo;
                        INLOG((inlog, "%3d |WB |: Writing LO value %#x\n", pos, _mc->_lo));
                        DBG((debugLog, "<%llu> Writing to Lo, value: %#x\n", SIM_TIME,
                             local_mc._opResultLo));
                    }
                    if (local_mc._hiWPort) {
                        _mc->_hi = local_mc._opResultHi;
                        pipeline->id_ex.mc._hi = local_mc._opResultHi;
                        INLOG((inlog, "%3d |WB |: Writing HI value %#x\n", pos, _mc->_hi));
                        DBG((debugLog, "<%llu> Writing to Hi, value: %#x\n", SIM_TIME,
                             local_mc._opResultHi));
                    }
                }
            }

            // Magic, ignore
            _mc->_gpr[0] = 0;

            AWAIT_P_PHI1;  // @negedge

            if (!local_mc._isSyscall && !local_mc._isIllegalOp) {
                if (local_mc._writeREG) {
                    INLOG((inlog, "%3d |WB |: Writeback to reg %d value %#x\n", pos, local_mc._decodedDST, local_mc._opResultLo));
                }
            }

            if (local_mc._isSyscall) {
                pipeline->runningSyscall = false;
                _mc->_sim_exit = local_mc._sim_exit;
                INLOG((inlog, "%3d |WB |: SYSCALL FINISHED\n", pos));
            }

        } else {
            int pos = pipeline->mem_wb.mc.position;
            AWAIT_P_PHI1;  // @negedge
            INLOG((inlog, "%3d |WB |: Dead\n", pos));
        }
    }
}
