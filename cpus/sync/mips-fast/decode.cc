#include "decode.h"
#include "common.h"
#include "mips.h"
#include "opcodes.h"
#include "app_syscall.h"

extern FILE *inlog;

Decode::Decode(Mipc *mc) {
    _mc = mc;
}

Decode::~Decode(void) {
}

bool Decode::doesThisRegHaveHazard(int reg, int ins) {
    MipsInsn i;
    i.data = ins;

    int onesrc, twosrc;
    bool subreg = false;

    switch (i.reg.op) {
        case 0:
        case 5:
            onesrc = i.reg.rs;
            twosrc = i.reg.rt;
            break;
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
        case 0xe:
            onesrc = i.imm.rs;
            twosrc = -7;
            break;
        case 0xf:
            onesrc = -7;
            twosrc = -7;
            break;
        case 2:
        case 3:
        case 4:
            onesrc = i.imm.rs;
            twosrc = i.imm.rt;
            break;
        case 1:
        case 6:
        case 7:
        case 0x20:
        case 0x21:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x31:
        case 0x39:
        case 0x28:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2e:
            onesrc = i.reg.rs;
            twosrc = -7;
            break;
        // SUBREG waale
        case 0x22:
        case 0x26:
            onesrc = i.reg.rs;
            twosrc = i.reg.rt;
            subreg = true;
            break;
    }

    pipeline->id_ex.mc.src1 = onesrc;
    pipeline->id_ex.mc.src2 = twosrc;

    // Handling subreg cases separately
    if (subreg) {
        pipeline->id_ex.mc.subreg = twosrc;
        return reg == onesrc;
    } else {
        pipeline->id_ex.mc.subreg = -7;
        return !(reg != onesrc && reg != twosrc);
    }
}

void Decode::MainLoop(void) {
    unsigned int ins;
    int _nfetched = 0, mypc;
    while (1) {
        AWAIT_P_PHI0;  // @posedge
        if (!pipeline->if_id._kill && !pipeline->runningSyscall) {

            if (pipeline->id_ex._skipExec)
                continue;

            ins = pipeline->if_id.mc._ins;
            mypc = pipeline->if_id.mc._pc;

            // Let it fetch pc+4.
            // If previous one was branch, pc would be updated
            // in the same +ve cycle by executor.cc
            MipsInsn i; i.data = ins;
            if(!pipeline->if_id._was_branch &&
               !(i.reg.op == 0 && i.reg.func == 0xc)) {
                _mc->_pc = _mc->_pc + 4;
            }

            AWAIT_P_PHI1;  // @negedge

            // Hazard detection
            // Checks for matching register
            // Previous instruction has to be a load instruction
            // The pipeline->id_ex.mc should still contain the old
            // values which we can easily reuse for this purpose
            if (doesThisRegHaveHazard(pipeline->id_ex.mc._decodedDST, ins) &&
                pipeline->id_ex.mc._memControl && pipeline->id_ex.mc._writeREG) {
                // HAZARD !!!
                pipeline->id_ex._skipExec = true;

                INLOG((inlog, "************HAZARD***********\n"));
            }
            // Hazard is checked

            // Now decode
            pipeline->id_ex.mc._ins = ins;
            pipeline->id_ex.mc._pc = mypc;
            pipeline->id_ex.mc.Dec(ins);

            INLOG((inlog, "%3d |D  |: ==> %s <==, requires %d, %d, dest is %d. Fetched from %#x\n",
                   _nfetched,
                   pipeline->id_ex.mc.insname.c_str(), pipeline->id_ex.mc.src1,
                   pipeline->id_ex.mc.src2, pipeline->id_ex.mc._decodedDST,
                   mypc));
            pipeline->id_ex.mc.position = _nfetched;

            _nfetched++;

            DBG((debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, ins));

            // So we can do the (mc->pc + 4) above
            pipeline->if_id._was_branch = pipeline->id_ex.mc._bd;

            if (pipeline->id_ex.mc._isSyscall) {
                pipeline->runningSyscall = TRUE;
            }

            pipeline->id_ex._kill = FALSE;

        } else {
            AWAIT_P_PHI1;  // @negedge
            pipeline->id_ex._kill = TRUE;
        }
    }
}
