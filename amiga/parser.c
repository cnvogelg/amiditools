#define __NOLIBBASE__
#include <proto/exec.h>
#include <midi/camd.h>

#include "compiler.h"
#include "debug.h"
#include "parser.h"

#define SysBase uh->sysBase

int parser_init(struct parser_handle *ph, struct ExecBase *sysBase, UBYTE port_num)
{
    ph->sysBase = sysBase;
    ph->port_num = port_num;
    ph->bytes_left = 0;
    ph->bytes_pos = 0;

    return 0;
}

void parser_exit(struct parser_handle *ph)
{

}

int parser_feed(struct parser_handle *ph, UBYTE data)
{
    // new command
    if(ph->bytes_left == 0) {
        int len = parser_midi_len(data);
        if(len == -1) {
            // invalid command
            D(("parser: invalid command %08lx\n", (ULONG)data));
            return 0;
        } else {
            // setup msg
            ph->msg = data << 24 | ph->port_num;
            if(len == 1) {
                return 1;
            } else {
                ph->bytes_left = len - 1;
                ph->bytes_pos = 16;
                return 0;
            }
        }
    }
    // data byte
    else {
        ph->msg |= data << ph->bytes_pos;
        ph->bytes_left --;
        ph->bytes_pos -= 8;
        if(ph->bytes_left == 0) {
            return 1;
        } else {
            return 0;
        }
    }
}

int parser_midi_len(UBYTE status)
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

