#ifndef MIDI_TOOLS_H
#define MIDI_TOOLS_H

/* mode */
extern void midi_tools_set_hex_mode(int on);
extern int  midi_tools_get_hex_mode(void);
extern void midi_tools_set_octave_middle_c(int oct);

/* parse tools */
extern LONG midi_tools_parse_number(char *str);
extern LONG midi_tools_parse_number_n(char *str, int max);
extern BYTE midi_tools_parse_number_7bit(char *str);
extern WORD midi_tools_parse_number_14bit(char *str);
extern BYTE midi_tools_parse_note(char *str);
extern LONG midi_tools_parse_timestamp(char *str);

extern void midi_tools_print_note(char *buf, UBYTE note, int use_sharps, int add_octave);

#endif
