#ifndef MIDI_TOOLS_H
#define MIDI_TOOLS_H

/* parse tools */
extern void midi_tools_set_hex_mode(int on);
extern void midi_tools_set_octave_middle_c(int oct);
extern LONG midi_tools_parse_number(char *str);
extern LONG midi_tools_parse_number_n(char *str, int max);
extern BYTE midi_tools_parse_number_7bit(char *str);
extern WORD midi_tools_parse_number_14bit(char *str);
extern BYTE midi_tools_parse_note(char *str);
extern LONG midi_tools_parse_timestamp(char *str);

#endif
