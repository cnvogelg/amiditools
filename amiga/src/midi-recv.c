#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>

#include <midi/camd.h>
#include <midi/mididefs.h>
#include <utility/tagitem.h>

#include "midi-setup.h"
#include "midi-tools.h"
#include "cmd.h"

static int handle_file(char *file_name);

static const char *TEMPLATE = 
    "V/VERBOSE/S,"
    "SMS/SYSEXMAXSIZE/K/N,"
    "CMDS/M";
typedef struct {
    LONG *verbose;
    ULONG *sysex_max_size;
    char **cmds;
} params_t;

struct DosLibrary *DOSBase;
struct UtilityBase *UtilityBase;
static params_t params;
static int verbose;
static struct MidiSetup midi_setup;
static struct MidiLink *rx;
/* command state */
static int midi_channel = 0; /* 0..15 */
static LONG last_timestamp = 0;

/* input handler */

static void handle_sysex(void)
{
    struct MidiNode *node = midi_setup.node;
    UBYTE *buf = midi_setup.sysex_buf;
    ULONG  buf_size = midi_setup.sysex_max_size;

    if(buf == NULL) {
        PutStr("sysex: SKIP: no buffer!\n");
        SkipSysEx(node);
    } else {
        ULONG size = QuerySysEx(node);
        if(size > buf_size) {
            Printf("sysex: SKIP: too large: %ld!\n", size);
            SkipSysEx(node);
        } else {
            ULONG got_size = GetSysEx(node, buf, buf_size);
            Printf("sysex(%ld): ", got_size);
            for(ULONG i=0;i<got_size;i++) {
                Printf("%02lx ", (ULONG)buf[i]);
            }
            PutStr("\n");
        }
    }
}

static void handle_system_msg(UBYTE status, UBYTE data1, UBYTE data2)
{ 
    if(status == MS_SysEx) {
        handle_sysex();
    }
}

/* channel voice messages */

static void handle_note_on(UBYTE chn, UBYTE note, UBYTE vel)
{
    Printf("channel  %2ld  note-on          %ld  %ld\n", chn+1, note, vel);
}

static void handle_note_off(UBYTE chn, UBYTE note, UBYTE vel)
{
    Printf("channel  %2ld  note-off         %ld  %ld\n", chn+1, note, vel);
}

static void handle_poly_pressure(UBYTE chn, UBYTE note, UBYTE vel)
{
    Printf("channel  %2ld  poly-pressure    %ld  %ld\n", chn+1, note, vel);
}

static void handle_control_change(UBYTE chn, UBYTE ctrl, UBYTE val)
{
    Printf("channel  %2ld  control-change   %ld  %ld\n", chn+1, ctrl, val);
}

static void handle_program_change(UBYTE chn, UBYTE prog)
{
    Printf("channel  %2ld  program-change   %ld\n", chn+1, prog);
}

static void handle_channel_pressure(UBYTE chn, UBYTE prog)
{
    Printf("channel  %2ld  channel-pressure %ld\n", chn+1, prog);
}

static void handle_pitch_bend(UBYTE chn, UBYTE least, UBYTE most)
{
    Printf("channel  %2ld  channel-pressure %ld  %ld\n", chn+1, least, most);
}

/* handle midi message */

static int check_channel(UBYTE channel)
{
    return (midi_channel == 0) || (midi_channel == channel);
}

static void handle_msg(UBYTE status, UBYTE data1, UBYTE data2)
{
    UBYTE grp = status & MS_StatBits;
    UBYTE chn = status & MS_ChanBits;
    switch(grp) {
        /* channel voice messages */
        case MS_NoteOff: /* NoteOff */
            if(check_channel(chn)) {
                handle_note_off(chn, data1, data2);
            }
            break;
        case MS_NoteOn: /* NoteOff */
            if(check_channel(chn)) {
                handle_note_on(chn, data1, data2);
            }
            break;
        case MS_PolyPress: /* Key Pressure */
            if(check_channel(chn)) {
                handle_poly_pressure(chn, data1, data2);
            }
            break;
        case MS_Ctrl: /* Control Change */
            if(check_channel(chn)) {
                handle_control_change(chn, data1, data2);
            }
            break;
        case MS_Prog: /* Program Change */
            if(check_channel(chn)) {
                handle_program_change(chn, data1);
            }
            break;
        case MS_ChanPress: /* Channel Pressure */
            if(check_channel(chn)) {
                handle_channel_pressure(chn, data1);
            }
            break;
        case MS_PitchBend:
            if(check_channel(chn)) {
                handle_pitch_bend(chn, data1, data2);
            }
            break;
        case MS_System:
            handle_system_msg(status, data1, data2);
            break;
        default:
            Printf("Invalid MIDI status: %02lx\n", status);
            break;
    }
}

/* --- commands --- */

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
    if((ch < 0) || (ch > 16)) {
        Printf("Invalid MIDI channel: %ld\n", ch);
        return 1;
    }
    midi_channel = ch;
    if(verbose) 
        Printf("channel: %ld\n", midi_channel);
    return 0;
}

/* misc */

static int cmd_file(int num_args, char **args)
{
    return handle_file(*args);
}

static int cmd_dev(int num_args, char **args)
{
    /* close old? */
    if(rx != NULL) {
        if(verbose)
            PutStr("closing midi device");
        midi_close(&midi_setup);
        rx = NULL;
    }

    /* midi open */
    midi_setup.rx_name = *args;
    if(verbose)
        Printf("opening midi device: %s\n", *args);
    int res = midi_open(&midi_setup);
    if(res != 0) {
        midi_close(&midi_setup);
        Printf("Error opening midi device: %s\n", *args);
        return res;
    }
    rx = midi_setup.rx_link;
    return 0;
}

/* command table */
static cmd_t command_table[] = {
    { "hex", "hexadecimal", 0, cmd_hex },
    { "dec", "decimal", 0, cmd_dec },
    { "omc", "octave-middle-c", 1, cmd_omc },
    { "ch", "channel", 1, cmd_ch },
    /* misc */
    { "file", NULL, 1, cmd_file },
    { "dev", "device", 1, cmd_dev },
    { NULL, 0, NULL } /* terminator */
};

static char **handle_other(char **args)
{
    // try a time stamp
    if(handle_file(*args)==0) {
        return args+1;
    }
    else {
        return NULL;
    }
}

static int handle_file(char *file_name)
{
    // does file exist?
    BPTR lock = Lock(file_name, ACCESS_READ);
    if(lock == NULL) {
        Printf("File not found: %s\n", file_name);
        return 1;
    }
    UnLock(lock);

    if(verbose) {
        Printf("Executing commands from: %s\n", file_name);
    }

    return cmd_exec_file(file_name, command_table, handle_other);
}

static int run(char **cmd_line, ULONG sysex_max_size)
{
    MidiMsg msg;
    BOOL alive = TRUE;
    ULONG sigmask;

    midi_setup.sysex_max_size = sysex_max_size;
    midi_setup.tx_name = NULL;
    rx = NULL;

    /* parse commands */
    int retcode = 0;
    char **result = cmd_exec_cmd_line(cmd_line, command_table, handle_other);
    if(result != NULL) {
        Printf("Failed parsing cmd: %s\n", *result);
        retcode = RETURN_ERROR;
    }

    if(rx == NULL) {
        Printf("ERROR: no input midi device given! use 'dev'\n");
        return 1;
    }

    if(verbose)
        Printf("midi-recv: from '%s'\n", midi_setup.rx_name);

    /* main loop */
    struct MidiNode *node = midi_setup.node;
    while(alive) {
        sigmask = Wait(SIGBREAKF_CTRL_C | 1L<<midi_setup.rx_sig);
        if(sigmask & SIGBREAKF_CTRL_C)
            alive=FALSE;
        while(GetMidi(midi_setup.node, &msg)) {
            if(verbose)
                Printf("%08ld: %08lx\n", msg.mm_Time, msg.mm_Msg);
            handle_msg(msg.mm_Status, msg.mm_Data1, msg.mm_Data2);
        }
    }

    midi_close(&midi_setup);
    return 0;
}

int main(int argc, char **argv)
{
    struct RDArgs *args;
    int result = RETURN_ERROR;

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

                /* max size for sysex */
                ULONG sysex_max_size = 2048;
                if(params.sysex_max_size != NULL) {
                    sysex_max_size = *params.sysex_max_size;
                }

                result = run(cmds, sysex_max_size);
            }

            FreeArgs(args);
            CloseLibrary((struct Library *)UtilityBase);
        }
    }
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}
