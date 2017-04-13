#include "executor.h"
#include "opcodes.h"
#include "common.h"

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

            if (!isSyscall && !isIllegalOp) {
                // TODO: Think about _lo, _hi
                // TODO: This place is doomed.

                // Fetch latest value of SRC1
                pipeline->getBypassValue(
                    ex_pipe.mc.src1,
                    &(ex_pipe.mc._decodedSRC1));

                // Fetch latest value of SRC2
                pipeline->getBypassValue(
                    ex_pipe.mc.src2,
                    &(ex_pipe.mc._decodedSRC2));

                // Fetch latest value of SUBREG
                pipeline->getBypassValueUnsigned(
                    ex_pipe.mc.subreg,
                    &(ex_pipe.mc._subregOperand));

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

            // Update value of bypasses if this // generated a result
            if (!ex_pipe.mc._memControl && ex_pipe.mc._writeREG) {
                // TODO: Also store _lo, _hi, _opResultHi
                pipeline->ex_mem.bypass.storeValueFromEx(
                    ex_pipe.mc._opResultLo, ex_pipe.mc._decodedDST);
            } else {
                pipeline->ex_mem.bypass.invalidateEx();
            }

            MipsInsn i; i.data = ins;
            INLOG((inlog,
                   "%3d |EX |: %s takes: %d, %d and %d (imm) to write %d into %d\n",
                   ex_pipe.mc.position, ex_pipe.mc.insname.c_str(),
                   ex_pipe.mc.src1 == -1 ? -1 : ex_pipe.mc._decodedSRC1,
                   ex_pipe.mc.src2 == -1 ? -1 : ex_pipe.mc._decodedSRC2,
                   i.imm.imm,
                   (!ex_pipe.mc._memControl && ex_pipe.mc._writeREG) ?
                   ex_pipe.mc._opResultLo : (unsigned int)(-1),
                   ex_pipe.mc._decodedDST));


            // IMP TODO: We need to send all info from ex_pipe to next
            // stage ki pipe. See if this works
            pipeline->ex_mem.mc = ex_pipe.mc;

            // pipeline->ex_mem.copyFromPipe(&ex_pipe);
            // pipeline->ex_mem._MAR = _mc->_MAR;
            // pipeline->ex_mem._opResultLo = _mc->_opResultLo;
            // pipeline->ex_mem._opResultHi = _mc->_opResultHi;
            // pipeline->ex_mem._btaken = _mc->_btaken;
            cout << "No Insn here ex" << ex_pipe.mc._pc << endl;
        } else {
            AWAIT_P_PHI1;  // @negedge
            pipeline->ex_mem._kill = TRUE;
        }
    }
}
