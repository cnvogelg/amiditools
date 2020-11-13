/*     
(c) Copyright 1988-1994 Microsoft Corporation.

Microsoft Bars&Pipes Professional Source Code License

This License governs use of the accompanying Software.
Microsoft hopes you find this Software useful.

You are licensed to do anything you want with the Software.

In return, we simply require that you agree:

1.      Not to remove any copyright notices from the Software.

2.      That the Software comes "as is", with no warranties.
        None whatsoever. This means no implied warranty of merchantability or
        fitness for a particular purpose or any warranty of non-infringement.
        Also, you must pass this disclaimer on whenever you distribute the Software.

3.      That we will not be liable for any of those types of damages known as indirect,
        special, consequential, or incidental related to the Software or this License,
        to the maximum extent the law permits. Also, you must pass this limitation of
        liability on whenever you distribute the Software.

4.      That if you sue anyone over patents that you think may apply to the Software
        for that person s use of the Software, your license to the Software ends automatically.

5.      That the patent rights Microsoft is licensing only apply to the Software,
        not to any derivatives you make.

6.      That your rights under the License end automatically if you breach this in any way.
*/

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/camd.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>
#include <v.h>
#include <tools.h>

#define MakeID(a,b,c,d)  ( (LONG)(a)<<24L | (LONG)(b)<<16L | (c)<<8 | (d) )

#define ID_CAMO         MakeID(CAMD_NAME[0],CAMD_NAME[1],'O',CAMD_PORT[0])

#pragma msg 62 ignore push

#include "midiinw.c"

#pragma msg 62 pop

struct MIDITool
{
  struct Tool tool;
  unsigned short status;
  unsigned short lownote, highnote;
  struct MidiLink *midilink;
  char location[100];
};

struct Library *CamdBase;

static UWORD chip MidiOut[]={
/*-------- plane # 0 --------*/

  0x0000,  0x0000,
  0xffff,  0xfc00,
  0xc000,  0x0c00,
  0xc000,  0x0c00,
  0xc000,  0x0c00,
  0xc000,  0x0c00,
  0xc400,  0x4c00,
  0xc102,  0x0c00,
  0xc030,  0x0c00,
  0xc000,  0x0c00,
  0xffff,  0xfc00,
  0x0000,  0x0000,

/*-------- plane # 1 --------*/

  0x0000,  0x0000,
  0x0000,  0x0c00,
  0x3e31,  0xfc00,
  0x3030,  0x3c00,
  0x2000,  0x1f00,
  0x0000,  0x0f00,
  0x0000,  0x0f00,
  0x2000,  0x1f00,
  0x3000,  0x3c00,
  0x3e01,  0xfc00,
  0x7fff,  0xfc00,
  0x0000,  0x0000
};


static struct Image MidiOutimage=
{
  0,0,24,12,2,
  MidiOut,
  0x3,0x0,NULL
};


/* Compiler glue: stub functions for camd.library */
struct MidiNode *CreateMidi(Tag tag, ...)
{	return CreateMidiA((struct TagItem *)&tag );
}

BOOL SetMidiAttrs(struct MidiNode *mi, Tag tag, ...)
{	return SetMidiAttrsA(mi, (struct TagItem *)&tag );
}


struct MidiLink *AddMidiLink(struct MidiNode *mi, LONG type, Tag tag, ...)
{	return AddMidiLinkA(mi, type, (struct TagItem *)&tag );
}


extern struct Functions *functions;

static void removetool(void)
{
  --functions->CAMD_count;
  if(functions->CAMD_count==0)
  {
    DeleteMidi(functions->CAMD_Node);
    functions->CAMD_Node=NULL;
  }
  CloseLibrary(CamdBase);
}


struct Tool *createtoolcode(struct MIDITool *copy)
{
  struct MIDITool *tool=(struct MIDITool *)functions->myalloc(sizeof(struct MIDITool),MEMF_CLEAR);

  if(tool)
  {
    if(copy)
    {
      *tool=*copy;
      tool->tool.next=0;
    }
    else
    {
      strcpy(tool->location,CAMD_NAME ".out." CAMD_PORT);
      tool->tool.tooltype=TOOL_OUTPUT|TOOL_ONTIME;
      tool->tool.toolid=ID_CAMO;
      tool->status=0x7F;
      tool->tool.touched=TOUCH_INIT;
    }
    tool->midilink=AddMidiLink(functions->CAMD_Node,MLTYPE_Sender,
                               MLINK_Comment,"B&P Pro CAMD Out Link",
                               MLINK_Location,tool->location,
                               MLINK_UserData,tool,
                               TAG_END);
  }
  return (struct Tool *)tool;
}

static struct Tool *loadtool(long file,long size)
{
  struct MIDITool *tool=(struct MIDITool *)functions->myalloc(sizeof(struct MIDITool),MEMF_CLEAR);

  if(tool)
  {
    functions->fastread(file,(char *)tool,sizeof(struct MIDITool));
    size-=sizeof(struct MIDITool);
  }
  tool->midilink=AddMidiLink(functions->CAMD_Node,MLTYPE_Sender,
                             MLINK_Comment,"B&P Pro CAMD Out Link",
                             MLINK_Location,tool->location,
                             MLINK_UserData,tool,
                             TAG_END);
  functions->fastseek(file,size,0);
  return (struct Tool *)tool;
}

void deletetool(struct MIDITool *tool)
{
  if(tool->midilink)
      RemoveMidiLink(tool->midilink);
  functions->myfree((void *)tool,sizeof(struct MIDITool));
}

extern struct NewWindow midiinNewWindowStructure1;

static struct Menu TitleMenu = {
    NULL,0,0,0,0,MENUENABLED,0,NULL
};

void edittoolcode(struct MIDITool *tool)
{
    register struct IntuiMessage *message;
    register struct Window *window;
    register long class, code;
    struct Gadget *gadget;
    struct NewWindow *newwindow;
    char menuname[100];
    midiinNewWindowStructure1.Screen = functions->screen;
    if (tool->tool.touched & TOUCH_EDIT) {
        midiinNewWindowStructure1.LeftEdge = tool->tool.left;
        midiinNewWindowStructure1.TopEdge = tool->tool.top;
        midiinNewWindowStructure1.Width = tool->tool.width;
        midiinNewWindowStructure1.Height = tool->tool.height;
    }
    if (!(tool->tool.touched & TOUCH_INIT)) tool->status = 0x7F;
    newwindow = (struct NewWindow *)
        (*functions->DupeNewWindow)(&midiinNewWindowStructure1);
    if (!newwindow) return;
    newwindow->Title = 0;
    newwindow->Flags |= BORDERLESS;
    newwindow->Flags &= ~0xF;
    window = (struct Window *) (*functions->FlashyOpenWindow)(newwindow);
    if (!window) return;
    functions->EmbossWindowOn(window,WINDOWCLOSE|WINDOWDEPTH|WINDOWDRAG,
        "Midi Out",0xffff,0xffff,0,0);
    tool->tool.window = window;
    functions->EmbossOn(window,1,1);
    functions->EmbossOn(window,2,1);
    functions->EmbossOn(window,3,1);
    functions->EmbossOn(window,4,1);
    functions->EmbossOn(window,5,1);
    functions->EmbossOn(window,6,1);
    functions->EmbossOn(window,7,1);
    functions->SelectEmbossed(window,1,tool->status & 1);
    functions->SelectEmbossed(window,2,tool->status & 4);
    functions->SelectEmbossed(window,3,tool->status & 8);
    functions->SelectEmbossed(window,4,tool->status & 16);
    functions->SelectEmbossed(window,5,tool->status & 32);
    functions->SelectEmbossed(window,6,tool->status & 64);
    functions->SelectEmbossed(window,7,tool->status & 128);
    strcpy(menuname,"MIDI Out v3.0 � 1993 The Blue Ribbon SoundWorks");
    TitleMenu.MenuName = menuname;
    SetMenuStrip(window,&TitleMenu);
    for (;;) {
        message = (struct IntuiMessage *)(*functions->GetIntuiMessage)(window);
        class = message->Class;
        code = message->Code;
        gadget = (struct Gadget *) message->IAddress;
        class = (*functions->SystemGadgets)(window,class,gadget,code);
        ReplyMsg((struct Message *)message);
        if (class == CLOSEWINDOW) break;
        else if (class == GADGETUP) {
            class = gadget->GadgetID;
            switch (class) {
                case 1 :
                    tool->status ^= 3;
                    (*functions->SelectEmbossed)(window,1,tool->status & 1);
                    break;
                case 2 :
                case 3 :
                case 4 :
                case 5 :
                case 6 :
                case 7 :
                    class = 1 << (class);
                    tool->status ^= class;
                    (*functions->SelectEmbossed)
                        (window,gadget->GadgetID,tool->status & class);
                    break;
            }
        }
    }
    ClearMenuStrip(window);
    functions->EmbossOff(window,1);
    functions->EmbossOff(window,2);
    functions->EmbossOff(window,3);
    functions->EmbossOff(window,4);
    functions->EmbossOff(window,5);
    functions->EmbossOff(window,6);
    functions->EmbossOff(window,7);
    functions->EmbossWindowOff(window);
    tool->tool.window = 0;
    tool->tool.left = window->LeftEdge;
    tool->tool.top = window->TopEdge;
    tool->tool.width = window->Width;
    tool->tool.height = window->Height;
    tool->tool.touched = TOUCH_INIT | TOUCH_EDIT;
    functions->FlashyCloseWindow(window);
    functions->DeleteNewWindow(newwindow);
}

static struct Event *processeventcode(struct Event *event)
{
  struct MIDITool *tool=(struct MIDITool *)event->tool;
  struct String *string;

  union
  {
    char buf[4];
    unsigned long l;
  } un={{0}};

  unsigned char status=event->status;

  event->tool=NULL;
  if(status<MIDI_SYSX)
  {
    if(!(tool->status & (1<<((event->status>>4)-8))))
    {
      if(!((event->status==MIDI_CCHANGE) && (event->byte1 > 119)))
          return event;
    }
  }
  else if(event->status==MIDI_SYSX)
  {
    if(tool->status&128)
    {
      string=((struct StringEvent *)event)->string;
      if(string && string->length>2 && tool->midilink)
          PutSysEx(tool->midilink,string->string);
    }
    return event;
  }
  if(tool->tool.track)
      status|=tool->tool.track->channelout;
  un.buf[0]=status;
  un.buf[1]=event->byte1;
  un.buf[2]=event->byte2;
  un.buf[3]=0;
  if(tool->midilink)
      PutMidi(tool->midilink,un.l);
  return event;
}

static struct ToolMaster master;

struct ToolMaster *inittoolmaster(void)
{
  CamdBase=OpenLibrary("camd.library",40L);
  if(CamdBase==NULL)
      return NULL;
  if(functions->CAMD_Node==NULL)
  {
    functions->CAMD_Node=CreateMidi(MIDI_Name,"Bars&Pipes Professional",
                                    MIDI_ClientType,CCType_Sequencer,
                                    MIDI_TimeStamp,&functions->timenow,
                                    TAG_END);
    if(functions->CAMD_Node==NULL)
    {
      CloseLibrary(CamdBase);
      return NULL;
    }
  }
  ++functions->CAMD_count;
  midiinNewWindowStructure1.LeftEdge = 500;
  memset((char *)&master,0,sizeof(struct ToolMaster));
  master.toolid = ID_CAMO;
  master.image = &MidiOutimage;
  strcpy(master.name,"CAMD " CAMD_NAME ".out." CAMD_PORT);
  master.toolsize = sizeof(struct MIDITool);
  master.createtool = createtoolcode;
  master.edittool = edittoolcode;
  master.processevent = processeventcode;
  master.deletetool=deletetool;
  master.loadtool=loadtool;
  master.removetool = removetool;
  master.tooltype = TOOL_OUTPUT | TOOL_ONTIME | TOOL_MIDI;
  return(&master);
}
