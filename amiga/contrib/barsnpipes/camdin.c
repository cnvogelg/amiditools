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
#include <clib/alib_protos.h>
#include <exec/types.h>
#include <exec/memory.h>

#include <string.h>

#include "v.h"
#include "tools.h"
#include "compilerspecific.h"

#define MakeID(a,b,c,d)  ( (LONG)(a)<<24L | (LONG)(b)<<16L | (c)<<8 | (d) )

#define ID_CAMI         MakeID(CAMD_NAME[0],CAMD_NAME[1],'I',CAMD_PORT[0])

#include "midiinw.c"

struct MIDITool
{
  struct Tool tool;
  unsigned short status;
  unsigned short lownote,highnote;
  struct MidiLink *midilink;
  char location[100];
};

static struct ToolMaster master;

static struct Hook hook;
struct Library *CamdBase;
static long sysexbufsize=32768;
static long msgqueue=8;

extern struct Functions *functions;

static UWORD CHIP MidiIn[]=
{
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

static struct Image MidiInimage=
{
  0,0,24,12,2,
  MidiIn,
  0x3,0x0,NULL
};

SAVEDS struct Tool *createtoolcode(struct MIDITool *copy)
{
  struct MIDITool *tool;

  tool=(struct MIDITool *)functions->myalloc(sizeof(struct MIDITool),MEMF_CLEAR);
  if(tool)
  {
    if(copy)
    {
      *tool=*copy;
      tool->tool.next=0;
    }
    else
    {
      strcpy(tool->location,CAMD_NAME ".in." CAMD_PORT);
      tool->tool.tooltype=TOOL_OUTPUT|TOOL_ONTIME;
      tool->tool.toolid=ID_CAMI;
      tool->status=0x7F;
      tool->tool.touched=TOUCH_INIT;
    }
    tool->midilink=AddMidiLink(functions->CAMD_Node,MLTYPE_Receiver,
                               MLINK_Comment,"B&P Pro CAMD In Link",
                               MLINK_Location,tool->location,
                               MLINK_UserData,tool,
                               TAG_END);
  }
  return (struct Tool *)tool;
}

SAVEDS static struct Tool *loadtool(long file,long size)
{
  struct MIDITool *tool=(struct MIDITool *)functions->myalloc(sizeof(struct MIDITool),MEMF_CLEAR);

  if(tool)
  {
    functions->fastread(file,(char *)tool,sizeof(struct MIDITool));
    size-=sizeof(struct MIDITool);
  }
  tool->midilink=AddMidiLink(functions->CAMD_Node,MLTYPE_Receiver,
                             MLINK_Comment,"B&P Pro CAMD In Link",
                             MLINK_Location,tool->location,
                             MLINK_UserData,tool,
                             TAG_END);
  functions->fastseek(file,size,0);
  return (struct Tool *)tool;
}

SAVEDS void deletetoolcode(struct MIDITool *tool)
{
  if(tool->midilink)
      RemoveMidiLink(tool->midilink);
  functions->myfree((char *)tool,sizeof(struct MIDITool));
}

static struct Menu TitleMenu = {
    NULL,0,0,0,0,MENUENABLED,0,NULL
};

SAVEDS void edittoolcode(struct MIDITool *tool)
{
  register struct IntuiMessage *message;
  register struct Window *window;
  register long class,code;
  struct Gadget *gadget;
  struct NewWindow *newwindow;
  char menuname[100];

  midiinNewWindowStructure1.Screen=functions->screen;
  if(tool->tool.touched & TOUCH_EDIT)
  {
    midiinNewWindowStructure1.LeftEdge = tool->tool.left;
    midiinNewWindowStructure1.TopEdge = tool->tool.top;
  }
  newwindow=(struct NewWindow *)functions->DupeNewWindow(&midiinNewWindowStructure1);
  if(!newwindow)
      return;
  newwindow->Title = 0;
  newwindow->Flags |= BORDERLESS;
  newwindow->Flags &= ~0xF;
  window = (struct Window *) (*functions->FlashyOpenWindow)(newwindow);
  if(!window)
      return;
  functions->EmbossWindowOn(window,WINDOWCLOSE|WINDOWDEPTH|WINDOWDRAG,
                            "CAMD In",0xffff,0xffff,0,0);
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
  strcpy(menuname,"CAMD In v1.0 � 1993 The Blue Ribbon SoundWorks");
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
  tool->tool.window=NULL;
  tool->tool.left=window->LeftEdge;
  tool->tool.top=window->TopEdge;
  tool->tool.touched=TOUCH_INIT | TOUCH_EDIT;
  functions->FlashyCloseWindow(window);
  functions->DeleteNewWindow(newwindow);
}

struct Event *processeventcode(struct Event *event)
{
  event->tool=event->tool->next;
  return event;
}

static struct Event eventarray[32];
static unsigned short eventindex;
struct Task *eventtask;
long eventsignal;

SAVEDS static void eventcode(void)
{
  struct Event *event;
  unsigned short index;
  struct Track *track;
  struct Event *copy;
  struct MIDITool *tool;
  char channel;
  unsigned char status;
  struct StringEvent *stringevent;

  eventsignal=1<<AllocSignal(-1);
  index=0;
  for(;;)
  {
    Wait(eventsignal);
    while(index!=eventindex)
    {
      event=&eventarray[index];
      status=1<<((event->status >> 4)-8);
      if(status==2)
      {
        if(!event->byte1)
        {
          ++index;
          index&=0x1F;
          continue;
        }
        if(event->byte2 && functions->remotecontrol[event->byte1])
        {
          if(functions->processinputevent)
          {
            functions->processinputevent(functions->remotecontrol[event->byte1]);
            ++index;
            index&=0x1F;
            continue;
          }
        }
      }
      if(status==128)
      {
        if(functions->multiin)
        {
          for(track=functions->tracklist ; track!=NULL ; track=track->next)
          {
            tool=(struct MIDITool *)track->toollist;
            if(tool->tool.toolid==ID_CAMI && (tool->status&128))
                break;
          }
        }
        else
            track=master.intrack;
        if(track)
        {
          tool=(struct MIDITool *)track->toollist;
          stringevent=(struct StringEvent *)functions->fastallocevent();
          if(stringevent)
          {
            stringevent->type=EVENT_SYSX;
            stringevent->tool=tool->tool.next;
            stringevent->time=event->time;
            if(!event->tool)
            {
              ++index;
              index&=0x1F;
              continue;
            }
            stringevent->string=(struct String *)functions->myalloc(event->data+3,0);
            if(stringevent->string)
            {
              stringevent->string->length=event->data+2;
              memcpy(stringevent->string->string,event->tool,event->data);
            }
            stringevent->status=MIDI_SYSX;
            functions->qevent((struct Event *)stringevent);
          }
        }
        ++index;
        index&=0x1F;
        continue;
      }
      if(functions->multiin)
      {
        channel=event->status & 15;
        for(track=functions->tracklist ; track!=NULL ; track=track->next)
        {
          tool=(struct MIDITool *)track->toollist;
          if(tool && (tool->tool.toolid==ID_CAMI) && (tool->status&status))
          {
            if(track->channelin==channel)
                break;
          }
        }
      }
      else
          track = master.intrack;
      if(track)
      {
        tool=(struct MIDITool *)track->toollist;
        if(tool && (tool->tool.toolid==ID_CAMI) && (tool->status&status))
        {
          copy=(struct Event *)functions->fastallocevent();
          if(copy)
          {
            copy->type=EVENT_VOICE;
            copy->tool=tool->tool.next;
            copy->status=(event->status & 0xF0);
            copy->byte1=event->byte1;
            copy->byte2=event->byte2;
            if(copy->status==MIDI_NOTEON && copy->byte2==0)
                copy->status = MIDI_NOTEOFF;
            copy->time=event->time;
            functions->qevent(copy);
          }
        }
      }
      ++index;
      index&=0x1F;
    }
  }
}

static ULONG ASMCALL SAVEDS midiinhook(REG(a0, struct Hook *hook),
                                       REG(a2, struct MidiLink *link),
                                       REG(a1, MidiMsg *msg),
                                       REG(d0, long sysexlen),
                                       REG(a3, void *sysexdata))
{
  struct Event *event;
  struct MIDITool *tool;

  if(msg->mm_Status>=0xf0)
      return 0;

  if(master.intrack==NULL)
      return 0;

  tool=(struct MIDITool *)master.intrack->toollist;

  if(link!=tool->midilink)
      return 0;

  event=&eventarray[eventindex];
  event->status=msg->mm_Status;
  event->byte1=msg->mm_Data1;
  event->byte2=msg->mm_Data2;
  ++eventindex;
  eventindex&=0x1F;
  event->time=functions->timenow;
  Signal(eventtask,eventsignal);
  return 0;
}

SAVEDS void removetoolcode(void)
{
  --functions->CAMD_count;
  if(functions->CAMD_count==0)
  {
    DeleteMidi(functions->CAMD_Node);
    functions->CAMD_Node=NULL;
  }
  CloseLibrary(CamdBase);
  DeleteTask(eventtask);
}

struct ToolMaster *inittoolmaster(void)
{
  CamdBase=OpenLibrary("camd.library",40L);
  if(CamdBase==NULL)
      return NULL;
  memset(&hook,0,sizeof(hook));
  hook.h_Entry=(void *)midiinhook;
  if(functions->CAMD_Node==NULL)
  {
    functions->CAMD_Node=CreateMidi(MIDI_Name,"Bars&Pipes Professional",
                                    MIDI_ClientType,CCType_Sequencer,
                                    MIDI_TimeStamp,&functions->timenow,
                                    MIDI_MsgQueue,msgqueue,
                                    MIDI_SysExSize,sysexbufsize,
                                    MIDI_RecvHook,&hook,
                                    TAG_END);
    if(functions->CAMD_Node==NULL)
    {
      CloseLibrary(CamdBase);
      return NULL;
    }
  }
  else
  {
    if(functions->CAMD_Node->mi_MsgQueueSize==0)
        SetMidiAttrs(functions->CAMD_Node,MIDI_MsgQueue,msgqueue,TAG_END);
    else
        msgqueue=functions->CAMD_Node->mi_MsgQueueSize;
    if(functions->CAMD_Node->mi_SysExQueueSize==0)
        SetMidiAttrs(functions->CAMD_Node,MIDI_SysExSize,sysexbufsize,TAG_END);
    else
        sysexbufsize=functions->CAMD_Node->mi_SysExQueueSize;
    SetMidiAttrs(functions->CAMD_Node,MIDI_RecvHook,&hook,TAG_END);
  }
  ++functions->CAMD_count;
  eventtask=CreateTask("camd in",40,(APTR)eventcode,4000);
  memset((char *)&master,0,sizeof(struct ToolMaster));
  master.toolid =ID_CAMI;
  master.image  =&MidiInimage;
  strcpy(master.name,"CAMD " CAMD_NAME ".in." CAMD_PORT);
  master.edittool=edittoolcode;
  master.processevent = processeventcode;
  master.createtool = createtoolcode;
  master.deletetool = deletetoolcode;
  master.loadtool = loadtool;
  master.tooltype = TOOL_INPUT | TOOL_MIDI;
  master.toolsize = sizeof(struct MIDITool);
  master.removetool = removetoolcode;
  return(&master);
}
