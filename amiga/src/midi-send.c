#define USE_INLINE_STDARG

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/camd.h>
#include <proto/utility.h>

#include <midi/camd.h>
#include <midi/mididefs.h>
#include <utility/tagitem.h>

#include "midi-setup.h"

static const char *TEMPLATE = 
    "OUT/A,"
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

struct DosLibrary *DOSBase;
struct UtilityBase *UtilityBase;
static params_t params;
static BOOL verbose;

/* command state */
static int hex_mode = 0;
static int midi_channel = 0; /* 0..15 */
static int octave_middle_c = 3;

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

    return 0;
}

/* command table */
static cmd_t commands[] = {
    { "hex", 0, cmd_hex },
    { "dec", 0, cmd_dec },
    { "omc", 1, cmd_omc },
    { "ch", 1, cmd_ch },
    { "on", 2, cmd_on },
    { "off", 2, cmd_off },
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

                result = run(params.out_cluster, cmds);
            }

            FreeArgs(args);

            CloseLibrary((struct Library *)UtilityBase);
        }
        CloseLibrary((struct Library *)DOSBase);
    }
    return result;
}
