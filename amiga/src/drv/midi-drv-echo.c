/*
 * a CAMD midi driver for classic Amigas
 */

#include <proto/exec.h>
#include <proto/alib.h>
#include <midi/camddevices.h>
#include <midi/camd.h>
#include <midi/mididefs.h>

#include "debug.h"
#include "compiler.h"
#include "midi-msg.h"
#include "midi-drv.h"

/* midi driver structure */
static struct MidiDeviceData my_dev = {
    .Magic = MDD_Magic,
    .Name = "echo",
    .IDString = "Echo driver",
    .Version = 0,
    .Revision = 1,
    .Init = midi_drv_init,
    .Expunge = midi_drv_expunge,
    .OpenPort = midi_drv_open_port,
    .ClosePort = midi_drv_close_port,
    .NPorts = MIDI_DRV_NUM_PORTS,
    .Flags = 1, // new style driver
};

extern struct ExecBase *SysBase;
static struct MsgPort *port;

struct MidiMessage {
    struct Message msg;
    midi_drv_msg_t drv_msg;
};

static struct MidiMessage *createMsg(midi_drv_msg_t *drv_msg)
{
    struct MidiMessage *mm = (struct MidiMessage *)AllocVec(sizeof(struct MidiMessage), 0);
    if(mm == NULL) {
        return NULL;
    }
    mm->msg.mn_Length = sizeof(struct MidiMessage);
    mm->drv_msg.port = drv_msg->port;
    mm->drv_msg.midi_msg = drv_msg->midi_msg;
    mm->drv_msg.priv_data = mm;
    
    // clone sysex
    ULONG size = drv_msg->sysex_size;
    UBYTE *buf = drv_msg->sysex_data;
    if((size > 0) && (buf != NULL)) {
        UBYTE *new_buf = (UBYTE *)AllocVec(size, 0);
        if(new_buf == NULL) {
            FreeVec(mm);
            return NULL;
        }
        CopyMem(buf, new_buf, size);
        mm->drv_msg.sysex_data = new_buf;
        mm->drv_msg.sysex_size = size;
    }

    return mm;
}

static void freeMsg(struct MidiMessage *mm)
{
    if(mm->drv_msg.sysex_data != NULL) {
        FreeVec(mm->drv_msg.sysex_data);
    }
    FreeVec(mm);
}

STRPTR midi_drv_api_config(void)
{
    return "midi.echo";
}

void midi_drv_api_tx_msg(midi_drv_msg_t *msg)
{
    struct MidiMessage *mm = createMsg(msg);
    if(mm != NULL) {
        PutMsg(port, (struct Message *)mm);
    }
}

int  midi_drv_api_rx_msg(midi_drv_msg_t **msg, ULONG *wait_got_mask)
{
    ULONG port_mask = 1<<port->mp_SigBit;
    ULONG wait_mask = port_mask | *wait_got_mask;
    ULONG ret_mask = Wait(wait_mask);

    // got a message?
    if((ret_mask & port_mask) == port_mask) {
        struct MidiMessage *mm = (struct MidiMessage *)GetMsg(port);
        if(mm != NULL) {
            *msg = &mm->drv_msg;
        }
        D(("rx: got midi msg: %lx\n", mm));
        return MIDI_DRV_RET_OK;
    }

    // return other signal
    *wait_got_mask = ret_mask;
    return MIDI_DRV_RET_OK_SIGNAL;
}

void midi_drv_api_rx_msg_done(midi_drv_msg_t *msg)
{
    struct MidiMessage *mm = (struct MidiMessage *)msg->priv_data;
    D(("rx: done midi msg: %lx\n", mm));
    freeMsg(mm);
}

int midi_drv_api_init(struct ExecBase *SysBase)
{
    port = CreatePort(NULL,0);
    if(port == NULL) {
        D(("no echo port!\n"));
        return MIDI_DRV_RET_MEMORY_ERROR;
    }
    return MIDI_DRV_RET_OK;
}

void midi_drv_api_exit(void)
{
    if(port != NULL) {
        DeletePort(port);
        port = NULL;
    }
}
