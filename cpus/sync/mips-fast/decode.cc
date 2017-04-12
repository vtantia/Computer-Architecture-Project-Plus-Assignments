#include "decode.h"
#include "common.h"
#include "mips.h"
#include "opcodes.h"
#include "app_syscall.h"

Decode::Decode(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->id_ex, 0, sizeof(pipeline->id_ex));
    pipeline->id_ex._kill = TRUE;
}

Decode::~Decode(void) {
}

bool Decode::doesThisRegHaveHazard(int reg, int ins) {
    MipsInsn i;
    i.data = ins;
    return !(reg != i.reg.rs && reg != i.reg.rt);
}

void Decode::MainLoop(void) {
    unsigned int ins;
    while (1) {
        if (!pipeline->if_id._kill) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;

            if (pipeline->id_ex._skipExec) {
                continue;
            }

            ins = pipeline->if_id._ins;

            if(!pipeline->if_id._was_branch) {
                _mc->_pc = _mc->_pc + 4;
            }

            DDBG;

            AWAIT_P_PHI1;  // @negedge

            DDBG;
            _mc->Dec(ins);

            // Hazard detection
            MipsInsn i;
            i.data = ins;
            if (doesThisRegHaveHazard(pipeline->id_ex._decodedDST, ins) &&
                _mc->_memControl && _mc->_writeREG) {
                // HAZARD !!!
                pipeline->id_ex._skipExec = true;
            }
            // Hazard is checked

            cout << "I got bd " << _mc->_bd << " for ins " << ins << endl;
            cout << "I got btgt " << _mc->_btgt << " for ins " << ins << endl;
#ifdef MIPC_DEBUG
            fprintf(debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, ins);
#endif

            pipeline->id_ex.copyFromMc(_mc);
            pipeline->id_ex._ins = ins;
            pipeline->if_id._was_branch = _mc->_bd;
            //_mc->_insValid = FALSE;
            //_mc->_decodeValid = TRUE;

            if (pipeline->id_ex._isSyscall) {
                pipeline->if_id._fetch_kill = TRUE;
                pipeline->if_id._kill = TRUE;
            }

            pipeline->id_ex._kill = FALSE;

        } else {
            AWAIT_P_PHI0;  // @posedge
            AWAIT_P_PHI1;  // @negedge
            pipeline->id_ex._kill = TRUE;
            // PAUSE(1);
        }
    }
}
