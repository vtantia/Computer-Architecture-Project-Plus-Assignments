#include "decode.h"
#include "common.h"

Decode::Decode(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->id_ex, 0, sizeof(pipeline->id_ex));
    pipeline->id_ex._isIllegalOp = TRUE;
}

Decode::~Decode(void) {
}

void Decode::MainLoop(void) {
    unsigned int ins;
    while (1) {
        if (true) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;
            ins = pipeline->if_id._ins;

            DDBG;
            AWAIT_P_PHI1;  // @negedge
            DDBG;
            _mc->Dec(ins);
#ifdef MIPC_DEBUG
            fprintf(debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, ins);
#endif
            pipeline->id_ex.copyFromMc(_mc);
            pipeline->id_ex._ins = ins;
            //_mc->_insValid = FALSE;
            //_mc->_decodeValid = TRUE;
        } else {
            PAUSE(1);
        }
    }
}
