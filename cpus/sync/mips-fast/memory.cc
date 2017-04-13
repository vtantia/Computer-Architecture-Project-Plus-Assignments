#include "memory.h"
#include "common.h"

Memory::Memory(Mipc *mc) {
    _mc = mc;
}

Memory::~Memory(void) {
}

void Memory::MainLoop(void) {
    while (1) {
        AWAIT_P_PHI0;  // @posedge
        if (!pipeline->ex_mem._kill) {

            // TODO: Verify that this is a deep copy
            EX_MEM mem_pipe = pipeline->ex_mem;

            void (*_memOp)(Mipc *) = mem_pipe.mc._memOp;

            // It could've generated _opResultLo any time, we
            // don't care. Thus, no check for memControl.
            if (pipeline->mem_wb.mc._writeREG) {
                getBypassValue(&(mem_pipe.mc), &(pipeline->mem_wb.mc));
            }

            AWAIT_P_PHI1;  // @negedge

            DBG((debugLog, "_memOp is %x\n", mem_pipe.mc._memOp));

            if (mem_pipe.mc._memControl) {
                mem_pipe.mc._memOp(&(mem_pipe.mc));

                if (mem_pipe.mc._writeREG) {
                    INLOG((inlog,
                           "%3d |MEM|: Write to reg %d from location %#x, value %#x\n",
                           mem_pipe.mc.position,
                           mem_pipe.mc._decodedDST,
                           mem_pipe.mc._MAR,
                           mem_pipe.mc._opResultLo));
                } else {
                    INLOG((inlog,
                           "%3d |MEM|: Some other memory op at MAR %#x\n",
                           mem_pipe.mc.position,
                           mem_pipe.mc._MAR));
                }

                DBG((debugLog, "<%llu> Access memory at addr %#x for ins %#x\n",
                     SIM_TIME, mem_pipe.mc._MAR, mem_pipe.mc._ins));
            } else {
                DBG((debugLog, "<%llu> Memory has nothing to do for ins %#x\n",
                     SIM_TIME, mem_pipe.mc._ins));
            }

            // Send info to next pipeline stage
            pipeline->mem_wb.mc = mem_pipe.mc;

            pipeline->mem_wb._kill = FALSE;
        } else {
            AWAIT_P_PHI1;  // @negedge
            pipeline->mem_wb._kill = TRUE;

            // If pipeline is stalled, next instruction should not
            // look for bypass values here
            pipeline->mem_wb.mc._writeREG = false;
        }
    }
}
