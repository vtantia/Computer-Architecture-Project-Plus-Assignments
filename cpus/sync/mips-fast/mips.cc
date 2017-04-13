#include "mips.h"
#include "common.h"
#include <assert.h>
#include "mips-irix5.h"

Mipc::Mipc() {
}

// Mipc::Mipc(Mipc *main_mc) {
//     _gpr = main_mc->_gpr;
//     _mem = main_mc->_mem;
// }

Mipc::Mipc(Mem *m) : _l('M') {
    _mem = m;
    _sys = new MipcSysCall(this);  // Allocate syscall layer

    _gpr = (unsigned int*)malloc(32 * sizeof(unsigned int));

    Reboot(ParamGetString("Mipc.BootROM"));
}

Mipc::~Mipc(void) {
}

void Mipc::MainLoop(void) {
    LL addr;
    unsigned int ins;  // Local instruction register

    Assert(_boot, "Mipc::MainLoop() called without boot?");

    _nfetched = 0;

    while (!_sim_exit) {
        if (!pipeline->if_id._fetch_kill) {

            AWAIT_P_PHI0;  // @posedge
            AWAIT_P_PHI1;  // @negedge

            DBG((debugLog, "<%llu> Fetched ins %#x from PC %#x\n", SIM_TIME, ins, _pc));

            addr = _pc;
            ins = _mem->BEGetWord(addr, _mem->Read(addr & ~(LL)0x7));
            _ins = ins;

            cout << "Instruction is " << ins << " at nfetch " << _nfetched <<
                " at PC " << _pc << endl;

            pipeline->if_id.mc._ins = ins;
            pipeline->if_id.mc._pc = addr;

            _nfetched++;
            _bd = 0;

            pipeline->if_id._kill = FALSE;

        } else {
            AWAIT_P_PHI0;  // @posedge
            AWAIT_P_PHI1;  // @negedge
            pipeline->if_id._kill = TRUE;
        }
    }

    MipcDumpstats();
    Log::CloseLog();

#ifdef MIPC_DEBUG
    assert(debugLog != NULL);
    fclose(debugLog);
#endif

    exit(0);
}

void Mipc::MipcDumpstats() {
    Log l('*');
    l.startLogging = 0;

    l.print("");
    l.print("************************************************************");
    l.print("");
    l.print("Number of instructions: %llu", _nfetched);
    l.print("Number of simulated cycles: %llu", SIM_TIME);
    l.print("CPI: %.2f", ((double)SIM_TIME) / _nfetched);
    l.print("Int Conditional Branches: %llu", _num_cond_br);
    l.print("Jump and Link: %llu", _num_jal);
    l.print("Jump Register: %llu", _num_jr);
    l.print("Number of fp instructions: %llu", _fpinst);
    l.print("Number of loads: %llu", _num_load);
    l.print("Number of syscall emulated loads: %llu", _sys->_num_load);
    l.print("Number of stores: %llu", _num_store);
    l.print("Number of syscall emulated stores: %llu", _sys->_num_store);
    l.print("");
}

void Mipc::fake_syscall(unsigned int ins) {
    _sys->pc = _pc;
    _sys->quit = 0;
    _sys->EmulateSysCall();
    if (_sys->quit)
        _sim_exit = 1;
}

/*------------------------------------------------------------------------
 *
 *  Mipc::Reboot --
 *
 *   Reset processor state
 *
 *------------------------------------------------------------------------
 */
void Mipc::Reboot(char *image) {
    FILE *fp;
    Log l('*');

    _boot = 0;

    if (image) {
        _boot = 1;
        printf("Executing %s\n", image);
        fp = fopen(image, "r");
        if (!fp) {
            fatal_error("Could not open `%s' for booting host!", image);
        }
        _mem->ReadImage(fp);
        fclose(fp);

        // Reset state
        _ins = 0;
        // _insValid = FALSE;
        // _decodeValid = FALSE;
        // _execValid = FALSE;
        // _memValid = FALSE;
        // _insDone = TRUE;

        _num_load = 0;
        _num_store = 0;
        _fpinst = 0;
        _num_cond_br = 0;
        _num_jal = 0;
        _num_jr = 0;

        _bd = 0;
        _btaken = 0;
        _btgt = 0xdeadbeef;
        _sim_exit = 0;
        _pc = ParamGetInt("Mipc.BootPC");  // Boom! GO
    }
}

LL MipcSysCall::GetDWord(LL addr) {
    _num_load++;
    return m->Read(addr);
}

void MipcSysCall::SetDWord(LL addr, LL data) {
    m->Write(addr, data);
    _num_store++;
}

Word MipcSysCall::GetWord(LL addr) {
    _num_load++;
    return m->BEGetWord(addr, m->Read(addr & ~(LL)0x7));
}

void MipcSysCall::SetWord(LL addr, Word data) {
    m->Write(addr & ~(LL)0x7, m->BESetWord(addr, m->Read(addr & ~(LL)0x7), data));
    _num_store++;
}

void MipcSysCall::SetReg(int reg, LL val) {
    _ms->_gpr[reg] = val;
}

LL MipcSysCall::GetReg(int reg) {
    return _ms->_gpr[reg];
}

LL MipcSysCall::GetTime(void) {
    return SIM_TIME;
}
