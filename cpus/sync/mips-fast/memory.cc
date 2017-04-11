#include "memory.h"
#include "common.h"

Memory::Memory(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->mem_wb, 0, sizeof(pipeline->mem_wb));
}

Memory::~Memory(void) {
}

void Memory::MainLoop(void) {
    Bool memControl;

    while (1) {
        if (true) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;
            memControl = pipeline->ex_mem._memControl;
            void (*_memOp)(Mipc *) = pipeline->ex_mem._memOp;

            DDBG;
            AWAIT_P_PHI1;  // @negedge
            DDBG;
            if (memControl) {
                _mc->_memOp(_mc);
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Accessing memory at address %#x for ins %#x\n",
                        SIM_TIME, _mc->_MAR, _mc->_ins);
#endif
            } else {
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Memory has nothing to do for ins %#x\n",
                        SIM_TIME, _mc->_ins);
#endif
            }
            //_mc->_execValid = FALSE;
            //_mc->_memValid = TRUE;
        } else {
            PAUSE(1);
        }
    }
}
