#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include <proto/utility.h>

#include <midi/camd.h>
#include <midi/mididefs.h>
#include <utility/tagitem.h>

#include "midi-setup.h"
#include "midi-tools.h"
#include "cmd.h"

#define DEFAULT_CLUSTER "mmp.out.0"

static const char *TEMPLATE = 
    "DEV/K,"
    "V/VERBOSE/S,"
    "SMS/SYSEXMAXSIZE/K/N,"
    "CMDS/M";
typedef struct {
    char *out_cluster;
    LONG *verbose;
    ULONG *sysex_max_size;
    char **cmds;
} params_t;

typedef union {
    LONG  cmd;
    UBYTE b[4];
} MidiCmd;

struct DosLibrary *DOSBase;
struct UtilityBase *UtilityBase;
static params_t params;
static BOOL verbose;
static struct MidiLink *tx;
/* command state */
static int midi_channel = 0; /* 0..15 */
static LONG last_timestamp = 0;

/* send midi */
static void midi3(UBYTE status, UBYTE data1, UBYTE data2)
{
    MidiCmd mc;
    mc.mm_Status = status;
    mc.mm_Data1 = data1;
    mc.mm_Data2 = data2;
    PutMidi(tx, mc.cmd);
}

static void midi2(UBYTE status, UBYTE data1)
{
    MidiCmd mc;
    mc.mm_Status = status;
    mc.mm_Data1 = data1;
    PutMidi(tx, mc.cmd);
}

static void midi1(UBYTE status)
{
    MidiCmd mc;
    mc.mm_Status = status;
    PutMidi(tx, mc.cmd);
}

static void midi_cc(UBYTE ctl, UBYTE val)
{
    midi3(MS_Ctrl | midi_channel, ctl, val);
}

static void midi_rpn(UWORD num, UWORD val)
{
    midi_cc(101, num >> 7);
    midi_cc(100, num & 0x7f);
    midi_cc(6, val >> 7);
    midi_cc(38, val & 0x7f);
    midi_cc(101, 0x7f);
    midi_cc(100, 0x7f);
}

static void midi_nrpn(UWORD num, UWORD val)
{
    midi_cc(99, num >> 7);
    midi_cc(98, num & 0x7f);
    midi_cc(6, val >> 7);
    midi_cc(38, val & 0x7f);
    midi_cc(101, 0x7f);
    midi_cc(100, 0x7f);
}

/* ---- commands ---- */

/* channel voice messages */

static int cmd_hex(int num_args, char **args)
{
    midi_tools_set_hex_mode(1);
    if(verbose)
        PutStr("hex\n");
    return 0;
}

static int cmd_dec(int num_args, char **args)
{
    midi_tools_set_hex_mode(0);
    if(verbose)
        PutStr("dec\n");
    return 0;
}

static int cmd_omc(int num_args, char **args)
{
    LONG omc = midi_tools_parse_number(args[0]);
    if((omc < 0) || (omc > 8)) {
        Printf("Invalid octave: %ld\n", omc);
        return 1;
    }
    midi_tools_set_octave_middle_c(omc);
    if(verbose)
        Printf("octave middle C: %ld\n", omc);
    return 0;
}

static int cmd_ch(int num_args, char **args)
{
    LONG ch = midi_tools_parse_number(args[0]);
    if((ch < 1) || (ch > 16)) {
        Printf("Invalid MIDI channel: %ld\n", ch);
        return 1;
    }
    midi_channel = ch - 1;
    if(verbose) 
        Printf("channel: %ld\n", midi_channel+1);
    return 0;
}

static int cmd_on(int num_args, char **args)
{
    BYTE note = midi_tools_parse_note(args[0]);
    BYTE velocity = midi_tools_parse_number_7bit(args[1]);
    if((note == -1) || (velocity == -1)) {
        return 1;
    }
    if(verbose)
        Printf("on: note=%ld/$%lx velocity=%ld/$%lx\n", note, note, velocity, velocity);

    midi3(MS_NoteOn | midi_channel, note, velocity);
    return 0;
}

static int cmd_off(int num_args, char **args)
{
    BYTE note = midi_tools_parse_note(args[0]);
    BYTE velocity = midi_tools_parse_number_7bit(args[1]);
    if((note == -1) || (velocity == -1)) {
        return 1;
    }
    if(verbose)
        Printf("off: note=%ld/$%lx velocity=%ld/$%lx\n", note, note, velocity, velocity);

    midi3(MS_NoteOff | midi_channel, note, velocity);
    return 0;
}

static int cmd_pp(int num_args, char **args)
{
    BYTE note = midi_tools_parse_note(args[0]);
    BYTE aftertouch = midi_tools_parse_number_7bit(args[1]);
    if((note == -1) || (aftertouch == -1)) {
        return 1;
    }
    if(verbose)
        Printf("poly pressure: note=%ld/$%lx aftertouch=%ld/$%lx\n", note, note, aftertouch, aftertouch);

    midi3(MS_PolyPress | midi_channel, note, aftertouch);
    return 0;
}

static int cmd_cc(int num_args, char **args)
{
    BYTE ctl = midi_tools_parse_number_7bit(args[0]);
    BYTE val = midi_tools_parse_number_7bit(args[1]);
    if((ctl == -1) || (val == -1)) {
        return 1;
    }
    if(verbose)
        Printf("control change: ctl=%ld/$%lx val=%ld/$%lx\n", ctl, ctl, val, val);

    midi_cc(ctl, val);
    return 0;
}

static int cmd_pc(int num_args, char **args)
{
    BYTE prg = midi_tools_parse_number_7bit(args[0]);
    if(prg == -1) {
        return 1;
    }
    if(verbose)
        Printf("program change: prg=%ld/$%lx\n", prg, prg);

    midi2(MS_Prog | midi_channel, prg);
    return 0;
}

static int cmd_cp(int num_args, char **args)
{
    BYTE aftertouch = midi_tools_parse_number_7bit(args[0]);
    if(aftertouch == -1) {
        return 1;
    }
    if(verbose)
        Printf("channel pressure: at=%ld/$%lx\n", aftertouch, aftertouch);

    midi2(MS_ChanPress | midi_channel, aftertouch);
    return 0;
}

static int cmd_pb(int num_args, char **args)
{
    WORD val = midi_tools_parse_number_14bit(args[0]);
    if(val == -1) {
        return 1;
    }
    if(verbose)
        Printf("pitch bend: val=%ld/$%lx\n", val, val);

    BYTE lsb = (BYTE)(val & 0x7f);
    BYTE msb = (BYTE)(val >> 7);
    midi3(MS_PitchBend | midi_channel, lsb, msb);
    return 0;
}

static int cmd_rpn(int num_args, char **args)
{
    WORD ctl = midi_tools_parse_number_14bit(args[0]);
    WORD val = midi_tools_parse_number_14bit(args[1]);
    if((ctl == -1) || (val == -1)) {
        return 1;
    }
    if(verbose)
        Printf("rpn: ctl=%5ld/$%ld val=%ld/$%lx\n", ctl, ctl, val, val);

    midi_rpn(ctl, val);
    return 0;
}

static int cmd_nrpn(int num_args, char **args)
{
    WORD ctl = midi_tools_parse_number_14bit(args[0]);
    WORD val = midi_tools_parse_number_14bit(args[1]);
    if((ctl == -1) || (val == -1)) {
        return 1;
    }
    if(verbose)
        Printf("nrpn: ctl=%5ld/$%ld val=%ld/$%lx\n", ctl, ctl, val, val);

    midi_nrpn(ctl, val);
    return 0;
}

static int cmd_panic(int num_args, char **args)
{
    if(verbose)
        PutStr("panic\n");

    for(UBYTE ch=0;ch<15;ch++) {
        midi3(MS_Ctrl | ch, 64, 0);
        midi3(MS_Ctrl | ch, 120, 0);
        midi3(MS_Ctrl | ch, 123, 0);
        for(UBYTE note=0;note<=127;note++) {
            midi3(MS_NoteOff | ch, note, 0);
        }
    }
    return 0;
}

/* system rt messages */

static int cmd_mc(int num_args, char **args)
{
    if(verbose)
        PutStr("midi clock\n");

    midi1(MS_Clock);
    return 0;
}

static int cmd_start(int num_args, char **args)
{
    if(verbose)
        PutStr("start\n");

    midi1(MS_Start);
    return 0;
}

static int cmd_stop(int num_args, char **args)
{
    if(verbose)
        PutStr("stop\n");

    midi1(MS_Stop);
    return 0;
}

static int cmd_cont(int num_args, char **args)
{
    if(verbose)
        PutStr("cont\n");

    midi1(MS_Continue);
    return 0;
}

static int cmd_as(int num_args, char **args)
{
    if(verbose)
        PutStr("active sensing\n");

    midi1(MS_ActvSense);
    return 0;
}

static int cmd_rst(int num_args, char **args)
{
    if(verbose)
        PutStr("reset\n");

    midi1(MS_Reset);
    return 0;
}

/* system common messages */

static int cmd_syx(int num_args, char **args)
{
    ULONG buf_len = 2 + num_args;
    UBYTE *data = AllocVec(buf_len, 0);
    if(data == NULL) {
        PutStr("Out of memory!\n");
        return 1;
    }

    /* prepare sysex buffer */
    data[0] = MS_SysEx;
    data[buf_len-1] = MS_EOX;
    UBYTE *ptr = data + 1;
    for(int i=0;i<num_args;i++) {
        BYTE val = midi_tools_parse_number_7bit(args[i]);
        if(val == -1) {
            FreeVec(data);
            return 2;
        }
        *ptr = val;
        ptr++;
    }

    if(verbose) {
        PutStr("sys ex: ");
        for(int i=0;i<buf_len;i++) {
            Printf("$%02lx ", data[i]);
        }
        PutStr("\n");
    }

    /* send sysex */
    PutSysEx(tx, data);

    /* free buffer */
    FreeVec(data);
    return 0;
}

static int cmd_syf(int num_args, char **args)
{
    BPTR fh = Open((STRPTR)*args, MODE_OLDFILE);
    if(fh == NULL) {
        Printf("Error opening sysex file: %s\n", *args);
        return 1;
    }

    /* get size */
    Seek(fh, 0, OFFSET_END);
    LONG buf_len = Seek(fh, 0, OFFSET_BEGINNING);

    UBYTE *data = AllocVec(buf_len, 0);
    if(data == NULL) {
        PutStr("Out of memory!\n");
        Close(fh);
        return 1;
    }

    int ret_code = 0;
    LONG got_size = Read(fh, data, buf_len);
    if(got_size == buf_len) {

        /* check if its a sysex file */
        if((data[0] == MS_SysEx) && (data[buf_len-1] == MS_EOX)) {
            PutSysEx(tx, data);
            if(verbose) {
                PutStr("sys ex: ");
                for(int i=0;i<buf_len;i++) {
                    Printf("$%02lx ", data[i]);
                }
                PutStr("\n");
            }
        } else {
            PutStr("No sysex file!\n");
            ret_code = 2;
        }

    } else {
        PrintFault(IoErr(), "ReadError");
        ret_code = 3;
    }

    FreeVec(data);

    Close(fh);
    return ret_code;
}

static int cmd_tc(int num_args, char **args)
{
    BYTE mt = midi_tools_parse_number_7bit(args[0]);
    BYTE val = midi_tools_parse_number_7bit(args[1]);
    if((mt == -1) || (val == -1)) {
        return 1;
    }
    if(verbose)
        Printf("time code: mt=%5ld/$%ld val=%ld/$%lx\n", mt, mt, val, val);

    BYTE s = mt << 4 | val;
    midi2(MS_QtrFrame, s);
    return 0;
}

static int cmd_spp(int num_args, char **args)
{
    WORD val = midi_tools_parse_number_14bit(args[0]);
    if(val == -1) {
        return 1;
    }
    if(verbose)
        Printf("song position: val=%ld/$%lx\n", val, val);

    BYTE lsb = (BYTE)(val & 0x7f);
    BYTE msb = (BYTE)(val >> 7);
    midi3(MS_SongPos, lsb, msb);
    return 0;
}

static int cmd_ss(int num_args, char **args)
{
    BYTE val = midi_tools_parse_number_7bit(args[0]);
    if(val == -1) {
        return 1;
    }
    if(verbose)
        Printf("song select: val=%ld/$%lx\n", val, val);

    midi2(MS_SongSelect, val);
    return 0;
}

static int cmd_tun(int num_args, char **args)
{
    if(verbose)
        PutStr("tune request\n");

    midi1(MS_TuneReq);
    return 0;
}

/* command table */
static cmd_t command_table[] = {
    { "hex", "hexadecimal", 0, cmd_hex },
    { "dec", "decimal", 0, cmd_dec },
    { "omc", "octave-middle-c", 1, cmd_omc },
    { "ch", "channel", 1, cmd_ch },
    /* channel voice messages */
    { "on", "note-on", 2, cmd_on },
    { "off", "note-off", 2, cmd_off },
    { "pp", "poly-pressure", 2, cmd_pp },
    { "cc", "control-change", 2, cmd_cc },
    { "pc", "program-change", 1, cmd_pc },
    { "cp", "channel-pressure", 1, cmd_cp },
    { "pb", "pitch-bend", 1, cmd_pb },
    { "rpn", NULL, 2, cmd_rpn },
    { "nrpn", NULL, 2, cmd_nrpn },
    { "panic", NULL, 0, cmd_panic },
    /* system rt messages */
    { "mc", "midi-clock", 0, cmd_mc },
    { "start", NULL, 0, cmd_start },
    { "stop", NULL, 0, cmd_stop },
    { "cont", "continue", 0, cmd_cont },
    { "as", "active-sensing", 0, cmd_as },
    { "rst", "reset", 0, cmd_rst },
    /* system common messages */
    { "syx", "system-exclusive", -1, cmd_syx },
    { "syf", "system-exclusive-file", 1, cmd_syf },
    { "tc", "time-code", 2, cmd_tc },
    { "spp", "song-position", 1, cmd_spp },
    { "ss", "song-select", 1, cmd_ss },
    { "tun", "tune-request", 0, cmd_tun },
    { NULL, 0, NULL } /* terminator */
};

static int handle_timestamp(char *arg)
{
    int rel = 0;

    // relative time stamp
    if(*arg == '+') {
        rel = 1;
        arg++;
    }

    // parse time stamp
    LONG millis = midi_tools_parse_timestamp(arg);
    if(millis == -1) {
        return 0;
    }

    // relative
    if(rel) {
        LONG ticks = millis / 20; // 50 Hz
        if(verbose) {
            Printf("waiting ticks: %ld\n", ticks);
        }
        Delay(ticks);
        last_timestamp += millis;
    }
    // absolute
    else {
        // calc delta to last time stamp
        if(last_timestamp > 0) {
            LONG delta = millis - last_timestamp;
            LONG ticks = delta / 20;
            if(verbose) {
                Printf("waiting ticks: %ld\n", ticks);
            }
            Delay(ticks);
        }
        last_timestamp = millis;
    }

    return 1;
}

static int run(char *cluster, char **cmd_line, ULONG sysex_max_size)
{
    struct MidiSetup ms;
    MidiMsg msg;

    /* midi open */
    ms.sysex_max_size = sysex_max_size;
    ms.tx_name = cluster;
    ms.rx_name = NULL;
    int res = midi_open(&ms);
    if(res != 0) {
        midi_close(&ms);
        return res;
    }
    tx = ms.tx_link;

    /* main loop */
    if(verbose) {
        Printf("midi-send: to '%s'\n", cluster);
    }

    /* parse commands */
    int retcode = 0;
    while(1) {
        char **result = cmd_exec_cmd_line(cmd_line, command_table);
        if(result != NULL) {
            // try a time stamp
            if(handle_timestamp(*result)) {
                // skip time stamp and continue
                cmd_line = result + 1;
            } else {
                Printf("Failed parsing cmd: %s\n", *result);
                retcode = RETURN_ERROR;
                break;
            }
        } else {
            break;
        }
    }

    midi_close(&ms);
    return 0;
}

int main(int argc, char **argv)
{
    struct RDArgs *args;
    int result = RETURN_ERROR;

    /* Open Libs */
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0L);
    if(DOSBase != NULL) {
        UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library", 0);
        if(UtilityBase != NULL) {

            /* First parse args */
            args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
            if(args == NULL) {
                PrintFault(IoErr(), "Args Error");
                return RETURN_ERROR;
            }

            /* check params */
            char **cmds = params.cmds;
            if((cmds == NULL) || cmds[0]==NULL) {
                PutStr("No commands given!\n");
            } 
            /* run tool */
            else {
                if(params.verbose != NULL) {
                    verbose = 1;
                }

                /* pick cluster */
                char *cluster = DEFAULT_CLUSTER;
                if(params.out_cluster != NULL) {
                    cluster = params.out_cluster;
                }

                /* max size for sysex */
                ULONG sysex_max_size = 2048;
                if(params.sysex_max_size != NULL) {
                    sysex_max_size = *params.sysex_max_size;
                }

                result = run(cluster, cmds, sysex_max_size);
            }

            FreeArgs(args);

            CloseLibrary((struct Library *)UtilityBase);
        }
        CloseLibrary((struct Library *)DOSBase);
    }
    return result;
}
