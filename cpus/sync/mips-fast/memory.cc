#include "memory.h"
#include "common.h"

Memory::Memory(Mipc *mc) {
    _mc = mc;
    memset(&pipeline->mem_wb, 0, sizeof(pipeline->mem_wb));

    pipeline->mem_wb._kill = TRUE;
}

Memory::~Memory(void) {
}

void Memory::MainLoop(void) {
    Bool memControl;

    while (1) {
        if (!pipeline->ex_mem._kill) {

            AWAIT_P_PHI0;  // @posedge

            // TODO: Verify that this is a deep copy
            EX_MEM mem_pipe = pipeline->ex_mem;

            memControl = mem_pipe.mc._memControl;
            void (*_memOp)(Mipc *) = mem_pipe.mc._memOp;

            AWAIT_P_PHI1;  // @negedge

            DBG((stdout, "_memOp is %x\n", _mc->_memOp));

            if (memControl) {
                mem_pipe.mc._memOp(&(mem_pipe.mc));

                DBG((debugLog, "<%llu> Access memory at addr %#x for ins %#x\n",
                     SIM_TIME, mem_pipe.mc._MAR, mem_pipe.mc._ins));
            } else {
                DBG((debugLog, "<%llu> Memory has nothing to do for ins %#x\n",
                     SIM_TIME, mem_pipe.mc._ins));
            }

            // IMP TODO: We need to send all info from mem_pipe to next
            // stage ki pipe. See if this works
            pipeline->mem_wb.mc = mem_pipe.mc;
            // pipeline->mem_wb.copyFromPipe(&mem_pipe);

            pipeline->mem_wb._kill = FALSE;
        } else {
            AWAIT_P_PHI0;  // @posedge
            AWAIT_P_PHI1;  // @negedge
            pipeline->mem_wb._kill = TRUE;
        }
    }
}
