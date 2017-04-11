#include "decode.h"
#include "common.h"

Decode::Decode(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->id_ex, 0, sizeof(pipeline->id_ex));
    pipeline->id_ex._kill = TRUE;
}

Decode::~Decode(void) {
}

void Decode::MainLoop(void) {
    unsigned int ins;
    while (1) {
        if (!pipeline->if_id._kill) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;

            if(!pipeline->if_id._was_branch){
                _mc->_pc = _mc->_pc + 4;
            }
            ins = pipeline->if_id._ins;

            DDBG;
            AWAIT_P_PHI1;  // @negedge
            DDBG;
            _mc->Dec(ins);
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
