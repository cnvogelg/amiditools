#include <exec/types.h>

#include "midi-tools.h"

static int hex_mode = 0;
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

void midi_tools_set_hex_mode(int on)
{
    hex_mode = on;
}

int midi_tools_get_hex_mode(void)
{
    return hex_mode;
}

void midi_tools_set_octave_middle_c(int oct)
{
    octave_middle_c = oct;
}

LONG midi_tools_parse_number(char *str)
{
    return midi_tools_parse_number_n(str, 255);
}

/* parse a number */
LONG midi_tools_parse_number_n(char *str, int max)
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
    int len = 0;
    while((*str != '\0') && (len<max)) {
        int digit = get_digit(*str);
        if(digit == -1) {
            return -1;
        }
        if(digit >= base) {
            return -1;
        }
        result = result * base + digit;
        str++;
        len++;
    }
    return result;
}

BYTE midi_tools_parse_number_7bit(char *str)
{
    LONG val = midi_tools_parse_number(str);
    if((val < 0) || (val > 127)) {
        return -1;
    }
    return (BYTE)val;
}

WORD midi_tools_parse_number_14bit(char *str)
{
    LONG val = midi_tools_parse_number(str);
    if((val < 0) || (val > 0x3fff)) {
        return -1;
    }
    return (WORD)val;
}

BYTE midi_tools_parse_note(char *str)
{
    /* is it a number? */
    BYTE note = midi_tools_parse_number_7bit(str);
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
        octave = midi_tools_parse_number(str);
        if(octave == -1) {
            return -1;
        }
        octave *= sign;
    }
    /* map C-2 .. G8 to midi note */
    LONG note_byte = (octave + 5 - octave_middle_c) * 12 + note;
    if((note_byte < 0) || (note_byte > 127)) {
        return -1;
    }
    return (BYTE)note_byte;
}

LONG midi_tools_parse_timestamp(char *str)
{
    // format: hh:mm:ss.iii
    //         012345678901
    if(str[2] == ':' && str[5] == ':' && str[8] == '.' && str[12] == '\x0') {
        LONG hours = midi_tools_parse_number_n(str, 2);
        LONG mins = midi_tools_parse_number_n(str+3, 2);
        LONG secs = midi_tools_parse_number_n(str+6, 2);
        LONG millis = midi_tools_parse_number(str+9);
        if((hours == -1) || (mins == -1) || (secs == -1) || (millis == -1)) {
            return -1;
        }
        return ((hours * 60 + mins) * 60 + secs) * 1000 + millis;
    }
    // format: ss.iii
    //         012345
    else if(str[2] == '.' && str[6] == '\x0') {
        LONG secs = midi_tools_parse_number_n(str, 2);
        LONG millis = midi_tools_parse_number(str+3);
        if((secs == -1) || (millis == -1)) {
            return -1;
        }
        return secs * 1000 + millis;
    }
    else {
        return -1;
    }
}

static char *sharp_notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static char *flat_notes[] =  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

void midi_tools_print_note(char *buf, UBYTE note, int use_sharps, int add_octave)
{
    int rel_note = note % 12;
    int octave = (int)(note / 12);
    char *ptr = buf;
    char *note_str;

    if(use_sharps) {
        note_str = sharp_notes[rel_note];
    } else {
        note_str = flat_notes[rel_note];
    }

    /* copy note */
    int len = 0;
    while(*note_str) {
        *ptr++ = *note_str++;
        len++;
    }

    if(add_octave) {
        octave = octave + octave_middle_c - 5;
        if(octave < 0) {
            *ptr++ = '-';
            octave = -octave;
        }
        if(octave >= 10) {
            int o = octave / 10;
            *ptr++ = '0' + o;
            octave -= 10;
        }
        *ptr++ = '0' + octave;
    }

    if(len==1) {
        *ptr++ = ' ';
    }

    *ptr = '\0';
}

