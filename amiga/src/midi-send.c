#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include <proto/utility.h>

#include <midi/camd.h>
#include <midi/mididefs.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

#define DEFAULT_CLUSTER "mmp.out.0"

static const char *TEMPLATE = 
    "OUT/K,"
    "V/VERBOSE/S,"
    "CMDS/M";
typedef struct {
    char *out_cluster;
    LONG *verbose;
    char **cmds;
} params_t;

typedef struct {
    char *name;
    int   num_args;
    int   (*run_cmd)(int num_args, char **args);
} cmd_t;

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
static int hex_mode = 0;
static int midi_channel = 0; /* 0..15 */
static int octave_middle_c = 3;

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

/* parsing */

static int get_digit(char c)
{
    if((c >= '0') && (c <= '9')) {
        return c - '0';
    }
    if((c >= 'A') && (c <= 'F')) {
        return c - 'A' + 10;
    }
    if((c >= 'a') && (c <= 'f')) {
        return c - 'a' + 10;
    }
    return -1;
}

/* parse a number */
static LONG parse_number(char *str)
{
    int base = 10;

    /* hex? */
    if(str[0] == '$') {
        base = 16;
        str++;
    } 
    else if((str[0] == '0') && (str[1] == 'x')) {
        base = 16;
        str+=2;
    }
    /* dec? */
    else if(str[0] == '!') {
        str++;
    }
    else if((str[0] == '0') && (str[1] == 'd')) {
        str+=2;
    }
    /* else use mode */
    else if(hex_mode) {
        base = 16;
    }

    /* parse decimal */
    LONG result = 0;
    while(*str != '\0') {
        int digit = get_digit(*str);
        if(digit == -1) {
            return -1;
        }
        if(digit >= base) {
            return -1;
        }
        result = result * base + digit;
        str++;
    }
    return result;
}

static BYTE parse_number_7bit(char *str)
{
    LONG val = parse_number(str);
    if((val < 0) || (val > 127)) {
        return -1;
    }
    return (BYTE)val;
}

static WORD parse_number_14bit(char *str)
{
    LONG val = parse_number(str);
    if((val < 0) || (val > 0x3fff)) {
        return -1;
    }
    return (WORD)val;
}

static BYTE parse_note(char *str)
{
    /* is it a number? */
    BYTE note = parse_number_7bit(str);
    if(note != -1) {
        return note;
    }

    /* note name? */
    switch(*str) {
        case 'c': case 'C': note = 0; break;
        case 'd': case 'D': note = 2; break;
        case 'e': case 'E': note = 4; break;
        case 'f': case 'F': note = 5; break;
        case 'g': case 'G': note = 7; break;
        case 'a': case 'A': note = 9; break;
        case 'b': case 'B': note = 11; break;
        case 'h': case 'H': note = 11; break;
        default:
            Printf("Unknown note: %s\n", str);
            return -1;
    }
    str++;
    /* minor? */
    if(*str == 'b') {
        note -= 1;
        str++;
    }
    /* major? */
    else if(*str == '#') {
        note += 1;
        str++;
    }

    /* negative octave? */
    LONG sign = 1;
    if(*str == '-') {
        sign = -1;
        str++;
    }

    /* octave? */
    LONG octave = 0;
    if(*str != '\0') {
        octave = parse_number(str);
        if(octave == -1) {
            Printf("Unknown octave: %s\n", str);
            return -1;
        }
        octave *= sign;
    }
    /* map C-2 .. G8 to midi note */
    LONG note_byte = (octave + octave_middle_c + 2) * 12 + note;
    if((note_byte < 0) || (note_byte > 127)) {
        Printf("invalid note: %ld\n", note_byte);
        return -1;
    }
    return (BYTE)note_byte;
}

/* ---- commands ---- */

/* channel voice messages */

static int cmd_hex(int num_args, char **args)
{
    hex_mode = 1;
    if(verbose)
        PutStr("hex\n");
    return 0;
}

static int cmd_dec(int num_args, char **args)
{
    hex_mode = 0;
    if(verbose)
        PutStr("dec\n");
    return 0;
}

static int cmd_omc(int num_args, char **args)
{
    LONG omc = parse_number(args[0]);
    if((omc < 0) || (omc > 8)) {
        Printf("Invalid octave: %ld\n", omc);
        return 1;
    }
    octave_middle_c = omc;
    if(verbose)
        Printf("octave middle C: %ld\n", omc);
    return 0;
}

static int cmd_ch(int num_args, char **args)
{
    LONG ch = parse_number(args[0]);
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
    BYTE note = parse_note(args[0]);
    BYTE velocity = parse_number_7bit(args[1]);
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
    BYTE note = parse_note(args[0]);
    BYTE velocity = parse_number_7bit(args[1]);
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
    BYTE note = parse_note(args[0]);
    BYTE aftertouch = parse_number_7bit(args[1]);
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
    BYTE ctl = parse_number_7bit(args[0]);
    BYTE val = parse_number_7bit(args[1]);
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
    BYTE prg = parse_number_7bit(args[0]);
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
    BYTE aftertouch = parse_number_7bit(args[0]);
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
    WORD val = parse_number_14bit(args[0]);
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
    WORD ctl = parse_number_14bit(args[0]);
    WORD val = parse_number_14bit(args[1]);
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
    WORD ctl = parse_number_14bit(args[0]);
    WORD val = parse_number_14bit(args[1]);
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
        BYTE val = parse_number_7bit(args[i]);
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

static int cmd_tc(int num_args, char **args)
{
    BYTE mt = parse_number_7bit(args[0]);
    BYTE val = parse_number_7bit(args[1]);
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
    WORD val = parse_number_14bit(args[0]);
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
    BYTE val = parse_number_7bit(args[0]);
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
static cmd_t commands[] = {
    { "hex", 0, cmd_hex },
    { "dec", 0, cmd_dec },
    { "omc", 1, cmd_omc },
    { "ch", 1, cmd_ch },
    /* channel voice messages */
    { "on", 2, cmd_on },
    { "off", 2, cmd_off },
    { "pp", 2, cmd_pp },
    { "cc", 2, cmd_cc },
    { "pc", 1, cmd_pc },
    { "cp", 1, cmd_cp },
    { "pb", 1, cmd_pb },
    { "rpn", 2, cmd_rpn },
    { "nrpn", 2, cmd_nrpn },
    { "panic", 0, cmd_panic },
    /* system rt messages */
    { "mc", 0, cmd_mc },
    { "start", 0, cmd_start },
    { "stop", 0, cmd_stop },
    { "cont", 0, cmd_cont },
    { "as", 0, cmd_as },
    { "rst", 0, cmd_rst },
    /* system common messages */
    { "syx", -1, cmd_syx },
    { "tc", 2, cmd_tc },
    { "spp", 1, cmd_spp },
    { "ss", 1, cmd_ss },
    { "tun", 0, cmd_tun },
    { NULL, 0, NULL } /* terminator */
};

static char **run_cmd(char **cmd_ptr)
{
    /* search command */
    char *cmd_name = cmd_ptr[0];
    cmd_ptr++;
    cmd_t *cmd = commands;
    while(cmd->name != NULL) {
        if(Stricmp(cmd->name, cmd_name)==0) {
            int num_args = cmd->num_args;
            char **args = cmd_ptr;
            /* skip args */
            if(num_args > 0) {
                for(int i=0;i<num_args;i++) {
                    /* command too short? */
                    if(*cmd_ptr == NULL) {
                        return NULL;
                    }
                    cmd_ptr++;
                }
            } 
            /* munge all args */
            else if(num_args < 0) {
                num_args = 0;
                while(*cmd_ptr != NULL) {
                    cmd_ptr++;
                    num_args++;
                }
            }
            /* run command */
            int result = cmd->run_cmd(num_args, args);
            if(result != 0) {
                return NULL;
            }
            return cmd_ptr;
        }
        cmd++;
    }
    /* command not found! */
    return NULL;
}

static int run(char *cluster, char **cmds)
{
    struct MidiSetup ms;
    MidiMsg msg;

    /* midi open */
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
    char **cmd = cmds;
    while(*cmd != NULL) {
        char **result = run_cmd(cmd);
        if(result == NULL) {
            Printf("ERROR: parsing command: '%s'!\n", *cmd);
            break;
        }
        cmd = result;
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

                result = run(cluster, cmds);
            }

            FreeArgs(args);

            CloseLibrary((struct Library *)UtilityBase);
        }
        CloseLibrary((struct Library *)DOSBase);
    }
    return result;
}
