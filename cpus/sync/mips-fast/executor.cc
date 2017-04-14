#include "executor.h"
#include "opcodes.h"
#include "common.h"
#include <assert.h>

Exe::Exe(Mipc *mc) {
    _mc = mc;
}

Exe::~Exe(void) {
}

void Exe::MainLoop(void) {
    unsigned int ins;
    Bool isSyscall, isIllegalOp;

    while (1) {
        AWAIT_P_PHI0;  // @posedge
        if (!pipeline->id_ex._kill) {

            if (pipeline->id_ex._skipExec) {
                // Internal thing. Ignore.
                AWAIT_P_PHI1;
                pipeline->id_ex._skipExec = false;

                pipeline->ex_mem._kill = true;
                continue;
            }

            // TODO: Verify that this is a deep copy
            ID_EX ex_pipe = pipeline->id_ex;
            ins = ex_pipe.mc._ins;

            isSyscall = ex_pipe.mc._isSyscall;
            isIllegalOp = ex_pipe.mc._isIllegalOp;

            int beforelo = -89, beforehi = -89;

            if (!isSyscall && !isIllegalOp) {
                // TODO: Think about _lo, _hi

                // It could've generated _opResultLo any time, we
                // don't care. Thus, no check for memControl.
                if (pipeline->mem_wb.mc._writeREG) {
                    getBypassValue(&(ex_pipe.mc), &(pipeline->mem_wb.mc));
                }

                // Previous instruction is still running MEM. We want
                // to know if it generated a value in EX, thus !memControl.
                if (!pipeline->ex_mem.mc._memControl &&
                    pipeline->ex_mem.mc._writeREG) {
                    getBypassValue(&(ex_pipe.mc), &(pipeline->ex_mem.mc));
                }

                // Bypass for lo
                if (pipeline->mem_wb.mc._loWPort) {
                    INLOG((inlog, "%3d |EX |: Got low value"));
                    ex_pipe.mc._lo = pipeline->mem_wb.mc._opResultLo;
                }
                if (pipeline->ex_mem.mc._loWPort) {
                    INLOG((inlog, "%3d |EX |: Got low value 2"));
                    ex_pipe.mc._lo = pipeline->ex_mem.mc._opResultLo;
                }

                // Bypass for hi
                if (pipeline->mem_wb.mc._hiWPort) {
                    ex_pipe.mc._hi = pipeline->mem_wb.mc._opResultHi;
                }
                if (pipeline->ex_mem.mc._hiWPort) {
                    ex_pipe.mc._hi = pipeline->ex_mem.mc._opResultHi;
                }

                beforelo = ex_pipe.mc._lo; beforehi = ex_pipe.mc._hi;

                MipsInsn i; i.data = ins;
                if (i.reg.op == 0 && (i.reg.func == 9 || i.reg.func == 8)) {
                    // This is jalr / jr. They use decodedSRC1 as
                    // their branch target, which wasn't updated by
                    // the bypass checks
                    ex_pipe.mc._btgt = ex_pipe.mc._decodedSRC1;
                }

                // Run executor
                // ex_pipe.mc returns which function to be run for
                // this instruction
                // ex_pipe.mc will now have result, btaken and other info
                ex_pipe.mc._opControl(&(ex_pipe.mc), ins);

                // Now handle if it is a branch
                if (ex_pipe.mc._bd) {
                    if (ex_pipe.mc._btaken) {
                        _mc->_pc = ex_pipe.mc._btgt;
                    } else {
                        _mc->_pc = _mc->_pc + 4;
                    }
                }

                DBG((debugLog, "<%llu> Executed ins %#x\n", SIM_TIME, ins));

            } else if (isSyscall) {
                DBG((debugLog, "<%llu> Deferring execution of syscall ins %#x\n",
                     SIM_TIME, ins));
            } else {
                DBG((debugLog, "<%llu> Illegal ins %#x in execution stage at PC %#x\n",
                     SIM_TIME, ins, _mc->_pc));
            }

            pipeline->ex_mem._kill = FALSE;
            AWAIT_P_PHI1;  // @negedge

            MipsInsn i; i.data = ins;
            INLOG((inlog,
                   "%3d |EX |: %s took: %d, %d and %d (imm)",
                   ex_pipe.mc.position, ex_pipe.mc.insname.c_str(),
                   ex_pipe.mc.src1 == -7 ? -99 : ex_pipe.mc._decodedSRC1,
                   ex_pipe.mc.src2 == -7 ? -99 : ex_pipe.mc._decodedSRC2,
                   i.imm.imm));

            if (ex_pipe.mc._writeREG) {
                INLOG((inlog,
                       "to write %#x into %d",
                       ex_pipe.mc._writeREG ? ex_pipe.mc._opResultLo : (unsigned int)(-6),
                       ex_pipe.mc._decodedDST));
            }

            INLOG((inlog, ". PC now is %#x\n", _mc->_pc));

            if (ex_pipe.mc.insname == "mflo" ||
                ex_pipe.mc.insname == "mfhi" ||
                ex_pipe.mc.insname == "mult") {
                INLOG((inlog,
                       "%3d |EX |: Before lo %#x, hi %#x. Now lo %#x hi %#x\n",
                       ex_pipe.mc.position,
                       beforelo, beforehi,
                       ex_pipe.mc._opResultLo, ex_pipe.mc._opResultHi));
            }

            pipeline->ex_mem.mc = ex_pipe.mc;

        } else {
            AWAIT_P_PHI1;  // @negedge
            pipeline->ex_mem._kill = TRUE;

            // If pipeline is stalled, next instruction should not
            // look for bypass values here
            pipeline->ex_mem.mc._writeREG = false;
        }
    }
}
