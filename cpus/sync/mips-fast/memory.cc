#include "memory.h"
#include "common.h"

Memory::Memory(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->mem_wb, 0, sizeof(pipeline->mem_wb));
}

Memory::~Memory(void) {
}

EX_MEM mem_pipe;

void Memory::MainLoop(void) {
    Bool memControl;

    while (1) {
        if (pipeline->ex_mem._ins != 0) {
            DDBG;
            AWAIT_P_PHI0;  // @posedge
            DDBG;
            mem_pipe = pipeline->ex_mem;
            memControl = mem_pipe._memControl;
            void (*_memOp)(Mipc *) = mem_pipe._memOp;

            DDBG;
            AWAIT_P_PHI1;  // @negedge
            DDBG;
#ifdef MIPC_DEBUG
            fprintf(stdout, "_memOp is %x\n", _mc->_memOp);
#endif
            if (memControl) {
                mem_pipe._memOp(_mc);
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Accessing memory at address %#x for ins %#x\n",
                        SIM_TIME, mem_pipe._MAR, mem_pipe._ins);
#endif
            } else {
#ifdef MIPC_DEBUG
                fprintf(debugLog, "<%llu> Memory has nothing to do for ins %#x\n",
                        SIM_TIME, mem_pipe._ins);
#endif
            }
            //_mc->_execValid = FALSE;
            //_mc->_memValid = TRUE;

            pipeline->mem_wb.copyFromPipe(&mem_pipe);

        } else {
            PAUSE(1);
        }
    }
}
