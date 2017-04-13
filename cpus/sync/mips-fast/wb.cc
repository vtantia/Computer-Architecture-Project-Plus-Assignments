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
        AWAIT_P_PHI0;
        if (!pipeline->mem_wb._kill) {

            Mipc local_mc = pipeline->mem_wb.mc;

            ins = local_mc._ins;

            cout << "No Insn " << pipeline->mem_wb.mc._pc << endl;
            int pos = pipeline->mem_wb.mc.position;

            if (isSyscall) {

                DBG((debugLog,
                     "<%llu> SYSCALL! Trapping to emulation layer at PC %#x\n",
                     SIM_TIME, _mc->_pc));

                local_mc._opControl(&(local_mc), ins);

            } else if (isIllegalOp) {

                printf("Illegal ins %#x at PC %#x. Terminating simulation!\n", ins,
                       local_mc._pc);
#ifdef MIPC_DEBUG
                fclose(debugLog);
#endif
                printf("Register state on termination:\n\n");
                _mc->dumpregs();
                exit(0);

            } else {

                cout << "Insn " << ins << " at pc " << local_mc._pc << " writeback " << local_mc._writeREG << endl;
                if (local_mc._writeREG) {
                    _mc->_gpr[local_mc._decodedDST] = local_mc._opResultLo;
                    cout << "Insn " << ins << " at pc " << local_mc._pc << " writes " <<
                        local_mc._opResultLo << " to register " << local_mc._decodedDST << endl;

                    DBG((debugLog, "<%llu> Writing to reg %u, value: %#x\n", SIM_TIME,
                         local_mc._decodedDST, local_mc._opResultLo));

                } else if (local_mc._writeFREG) {
                    _mc->_fpr[(local_mc._decodedDST) >> 1].l[FP_TWIDDLE ^ ((local_mc._decodedDST)&1)] =
                        local_mc._opResultLo;

                    DBG((debugLog, "<%llu> Writing to freg %u, value: %#x\n", SIM_TIME,
                         local_mc._decodedDST >> 1, local_mc._opResultLo));

                } else if (local_mc._loWPort || local_mc._hiWPort) {
                    if (local_mc._loWPort) {
                        _mc->_lo = local_mc._opResultLo;
                        DBG((debugLog, "<%llu> Writing to Lo, value: %#x\n", SIM_TIME,
                             opResultLo));
                    }
                    if (local_mc._hiWPort) {
                        _mc->_hi = local_mc._opResultHi;
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
                    INLOG((inlog, "%3d |WB |: Writing to %d value %d\n", pos, local_mc._decodedDST, local_mc._opResultLo));
                }
            }

            if (local_mc._isSyscall) {
                pipeline->if_id._fetch_kill = FALSE;
                INLOG((inlog, "%3d |WB |: SYSCALL FINISHED\n", pos));
            }

        } else {
            int pos = pipeline->mem_wb.mc.position;
            AWAIT_P_PHI1;  // @negedge
            INLOG((inlog, "%3d |WB |: Dead\n", pos));
        }
    }
}
