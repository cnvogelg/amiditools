#define __NOLIBBASE__
#include <proto/exec.h>
#include <midi/camd.h>
#include <midi/mididefs.h>

#include "compiler.h"
#include "debug.h"
#include "midi-parser.h"

#define SysBase ph->sysBase

int midi_parser_init(struct midi_parser_handle *ph, struct ExecBase *sysBase,
                     UBYTE port_num, ULONG max_sysex_size)
{
    ph->sysBase = sysBase;
    ph->port_num = port_num;
    ph->bytes_left = 0;
    ph->bytes_pos = 0;

    ph->sysex_buf = NULL;
    ph->sysex_bytes = 0;
    ph->sysex_left = 0;
    ph->sysex_max = max_sysex_size;

    return 0;
}

void midi_parser_exit(struct midi_parser_handle *ph)
{
    if(ph->sysex_buf != NULL) {
        FreeVec(ph->sysex_buf);
    }
}

static int sysex_data(struct midi_parser_handle *ph, UBYTE data)
{
    // store in buffer if some room is left
    if(ph->sysex_left > 0) {
        ph->sysex_buf[ph->sysex_bytes] = data;
        ph->sysex_left--;
    }
    // always count byte
    ph->sysex_bytes++;
    D(("parser: sysex data: %lx (left=%ld, bytes=%ld)\n", data,
       ph->sysex_left, ph->sysex_bytes));
    return MIDI_PARSER_RET_NONE;
}

static int sysex_begin(struct midi_parser_handle *ph)
{
    D(("parser: sysex begin\n"));

    // first use allocs buffer
    if(ph->sysex_buf == NULL) {
        ph->sysex_buf = AllocVec(ph->sysex_max, 0);
    }

    // bytes are only left if we got a buffer
    if(ph->sysex_buf != NULL) {
        ph->sysex_left = ph->sysex_max;
    } else {
        ph->sysex_left = 0;
        D(("parser: NO sysex mem!\n"));
    }
    ph->sysex_bytes = 0;

    // store first byte
    return sysex_data(ph, MS_SysEx);
}

static int sysex_end(struct midi_parser_handle *ph)
{
    // store EOX byte
    sysex_data(ph, MS_EOX);

    D(("parser: sysex end: size=%ld max=%ld\n", ph->sysex_bytes, ph->sysex_max));

    // did the whole sysex block fit into buffer?
    if((ph->sysex_buf == NULL) || (ph->sysex_bytes > ph->sysex_max)) {
        return MIDI_PARSER_RET_SYSEX_TOO_LARGE;
    } else {
        return MIDI_PARSER_RET_SYSEX_OK;
    }
}

static int handle_sysex(struct midi_parser_handle *ph, UBYTE data)
{
    // sysex begin?
    if(data == MS_SysEx) {
        return sysex_begin(ph);
    }
    // sysex end?
    else if(data == MS_EOX) {
        return sysex_end(ph);
    }
    // sysex data
    else if(((data & 0x80) == 0) && (ph->sysex_bytes > 0)) {
        return sysex_data(ph, data);
    }
    // no sysex. but a realtime message (that might interleave sysex)
    else if((data & 0xf8) == 0xf8) {
        return MIDI_PARSER_RET_INTERNAL;
    }
    // any other message
    else {
        // reset previous sysex state
        ph->sysex_bytes = 0;
        ph->sysex_left = 0;
        return MIDI_PARSER_RET_INTERNAL;
    }
}

int midi_parser_feed(struct midi_parser_handle *ph, UBYTE data)
{
    // new command
    if(ph->bytes_left == 0) {
        // check for sysex
        int res = handle_sysex(ph, data);
        if(res != MIDI_PARSER_RET_INTERNAL) {
            return res;
        }
        // its a status byte: message begin
        if((data & 0x80) == 0x80) {            
            int len = midi_parser_get_cmd_len(data);
            ph->last_len = len;
            ph->last_status = data;
            if(len == -1) {
                // invalid command
                D(("parser: invalid command %08lx\n", (ULONG)data));
                return MIDI_PARSER_RET_ERROR;
            } else {
                // setup msg
                ph->msg = data << 24 | ph->port_num;
                if(len == 1) {
                    return MIDI_PARSER_RET_MSG;
                } else {
                    ph->bytes_left = len - 1;
                    ph->bytes_pos = 16;
                    return MIDI_PARSER_RET_NONE;
                }
            }
        }
        // its data -> status byte was omitted, repeat last status
        else {
            ph->msg = ph->last_status << 24 | data << 16 | ph->port_num;
            if(ph->last_len == 2) {
                return MIDI_PARSER_RET_MSG;
            } else {
                ph->bytes_left = ph->last_len - 2;
                ph->bytes_pos = 8;
                return MIDI_PARSER_RET_NONE;
            }
        }
    }
    // data byte
    else {
        ph->msg |= data << ph->bytes_pos;
        ph->bytes_left --;
        ph->bytes_pos -= 8;
        if(ph->bytes_left == 0) {
            return MIDI_PARSER_RET_MSG;
        } else {
            return MIDI_PARSER_RET_NONE;
        }
    }
}

int midi_parser_get_cmd_len(UBYTE status)
{
    if((status & 0x80) == 0) {
        return -1;
    }

    switch(status & 0xf0){
		case 0x80:
		case 0x90:
		case 0xa0:
		case 0xb0:
		case 0xe0:
			return 3;

		case 0xc0:
		case 0xd0:
			return 2;

		case 0xf0:
			switch(status){
				case 0xf0:
					return -1; // sysex begin not handled here
				case 0xf1:
					return 2;
				case 0xf2:
					return 3;
				case 0xf3:
					return 2;
				case 0xf4:
					return -1;
				case 0xf5:
					return -1;
				case 0xf6:
					return 1;
				case 0xf7:
					return -1; // sysex end not handled here
                case 0xf9:
                    return -1;
                case 0xfd:
                    return -1;
				default:
					return 1;
			}
	}
    return -1;
}

