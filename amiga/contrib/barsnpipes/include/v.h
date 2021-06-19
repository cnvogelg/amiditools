/*     G
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

#ifndef BNP_V_H
#define BNP_V_H

#ifndef NO_DEBUG_MEMORY
#define AllocMem(a,b) myalloc(a,b)
#define FreeMem(a,b) myfree(a,b)
#endif

#define MAXTEMPO        350
#define MINTEMPO        10

#define MIDI_NOTEOFF    0x80
#define MIDI_NOTEON     0x90
#define MIDI_PTOUCH     0xA0
#define MIDI_CCHANGE    0xB0
#define MIDI_PCHANGE    0xC0
#define MIDI_MTOUCH     0xD0
#define MIDI_PBEND      0xE0
#define MIDI_SYSX       0xF0
#define MIDI_MTC        0xF1
#define MIDI_SONGPP     0xF2
#define MIDI_SONGS      0xF3
#define MIDI_EOX        0xF7
#define MIDI_CLOCK      0xF8
#define MIDI_START      0xFA
#define MIDI_CONTINUE   0xFB
#define MIDI_STOP       0xFC
#define MIDI_SENSE      0xFE

#define EVENT_ONTIME    0x10            /* This event is an on-time event. */
#define EVENT_BRANCH    0x20            /* This event is traversing a branch. */
#define EVENT_PADEDIT   0x40            /* Not a real time event. */
#define EVENT_PLAYONLY  0x80            /* Discard event when recording. */
#define EVENT_VOICE     1               /* Performance event */
#define EVENT_SYSX      2               /* System exclusive event. */
#define EVENT_LYRIC     3               /* Lyric string. */
#define EVENT_TIMESIG   4               /* Time signature change event. */
#define EVENT_KEY       5               /* Key change event. */
#define EVENT_CHORD     6               /* Chord change event. */
#define EVENT_RHYTHM    7               /* Rhythm template event. */
#define EVENT_DYNAMICS  8               /* Dynamics event. */
#define EVENT_SEED      9               /* Use to seed repeating tools. */


struct Event {
    struct Event *next;                 /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char status;               /* MIDI status. */
    unsigned char byte1;                /* First byte of data. */
    unsigned char byte2;                /* Second byte of data. */
    long data;                          /* Data storage. */
    struct Tool *tool;                  /* Tool that processes this next. */
};

struct EventList {
    struct Event *first;                /* First in list. */
    struct Event *point;                /* Current position in list. */
};


struct NoteEvent {
    struct NoteEvent *next;             /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char status;               /* MIDI status. */
    unsigned char value;                /* Note value. */
    unsigned char velocity;             /* Note velocity. */
    unsigned short duration;            /* The duration of this event. */
    short data;                         /* Data storage. */
    struct Tool *tool;                  /* Tool that processes this next. */
};

struct String {
    unsigned short length;              /* The length of the string that follows. */
    char string[1];                     /* This is actually a variable length array. */
};

struct StringEvent {
    struct StringEvent *next;           /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char status;               /* For SysEx. */
    short length;                       /* Display length. */
    struct String *string;              /* Pointer to string. */
    struct Tool *tool;                  /* Tool that processes this next. */
};

struct SysEx {
    unsigned long length;               /* The length of the packet. */
    struct Task *task;                  /* Sending task, for signaling. */
    short signal;                       /* Signal number. */
    char done;                          /* Done flag. */
    char *data;                         /* The data. */
};

struct SysExEvent {
    struct SysExEvent *next;            /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char status;               /* For SysEx. */
    short length;                       /* Display length. */
    struct SysEx *sysex;                /* Pointer to data. */
    struct Tool *tool;                  /* Tool that processes this next. */
};

struct Chord {
    struct Chord *next;                 /* Next in list. */
    struct String *name;                /* Chord name. */
    unsigned long pattern;              /* Bit pattern of chord. */
    unsigned short id;                  /* Chord identifier. */
    char count;                         /* Number of notes in chord. */
};

struct ChordEvent {
    struct ChordEvent *next;            /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char root;                 /* Root note. */
    char flats;                         /* Chord root is a flat. */
    char pad;
    unsigned long chordid;              /* Chord id. */
    struct Chord *chord;                /* Chord. */
};

struct KeyEvent {
    struct KeyEvent *next;              /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char root;                 /* Root note. */
    char flats;                         /* Display the key with flats. */
    char pad;
    unsigned long scaleid;              /* Scale id. */
    struct Chord *scale;                /* Pointer to Scale definition. */
};


struct TimeSigEvent {
    struct TimeSigEvent *next;          /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char beatcount;            /* Number of beats per measure. */
    unsigned char beat;                 /* What note gets beat. */
    char pad;
    unsigned short clocks;              /* Clocks per beat. */
    unsigned short measures;            /* Measures till next. */
    unsigned long totalclocks;          /* Clocks till next. */
};

struct TimeSigList {
    struct TimeSigEvent *first;         /* First in list. */
    struct TimeSigEvent *point;         /* Current position in list. */
    unsigned short measure;             /* Current measure. */
    unsigned short beat;                /* Current beat. */
    unsigned short clock;               /* Current clock. */
};

struct Rhythm {
    struct Rhythm *next;                /* Next in list of templates. */
    struct String *name;                /* Name of this rhythm. */
    struct NoteEvent *notes;            /* List of notes. */
    long length;                        /* Loop length. */
    short id;                           /* Identifier. */
};

struct RhythmEvent {
    struct RhythmEvent *next;           /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    char pad;                           /* Empty. */
    unsigned short rhythmid;            /* Id of rhythm clip. */
    struct Rhythm *rhythm;              /* Rhythm. */
    struct NoteEvent *point;            /* Current position in rhythm. */
};

struct DynamicsEvent {
    struct DynamicsEvent *next;         /* The next event in the list. */
    long time;                          /* When this event occurs. */
    char type;                          /* What type of event. */
    unsigned char value;                /* Dynamic value. */
    unsigned char lastvalue;            /* Previous dynamic value. */
    unsigned char nextvalue;            /* Next Dynamic value. */
    long lasttime;                      /* Previous time. */
    long nexttime;                      /* Next time. */
};


struct Clip {
    struct Clip *next;                  /* List of clips. */
    struct EventList events;            /* Event list. */
    struct EventList chords;            /* List of bass chords. */
    struct EventList keys;              /* List of keys. */
    struct EventList lyrics;            /* List of lyrics. */
    struct EventList rhythm;            /* List of rhythm templates. */
    struct EventList dynamics;          /* List of dynamic changes. */
    struct TimeSigList timesig;         /* List of time signatures. */
    struct String *name;                /* Name of this clip. */
    struct String *notes;               /* Notes for this clip. */
    long begin;                         /* Time this begins. */
    long end;                           /* Time this ends. */
    unsigned char highnote;             /* Highest note, for display. */
    unsigned char lownote;              /* Lowest note, for display. */
    char locked;                        /* Locked during record. */
};

/*      For handling a linked list of clips in parallel: */

struct ClipList {
    struct ClipList *next;              /* Next Clip or ClipList. */
    long cliplisttag;                   /* Tag to identify ClipList. */
    struct Clip *cliplist;              /* List of clips. */
};


struct Tool {
    struct Tool *next;                  /* Next tool used by this track. */
    struct Tool *branch;                /* Tool on other track. */
    struct Tool *parent;                /* Parent tool (for macros.) */
    struct ToolMaster *toolmaster;      /* Pointer to actual tool. */
    struct Clip *clip;                  /* Clip to be worked on. */
    struct String *name;                /* Name of this tool. */
    struct Window *window;              /* Edit window. */
    struct Track *track;                /* Track that owns this tool. */
    long toolid;                        /* Tool ID. */
    unsigned short left,top;            /* Position of edit window. */
    unsigned short width,height;        /* Size of edit window. */
    unsigned short x,y;                 /* Position in pipe display. */
    unsigned short xindex;              /* How far down list this is. */
    unsigned short yindex;              /* How far down track list this is. */
    short branchindex;                  /* How far away branch tool is. */
    unsigned short id;                  /* ID for file io. */
    char intool;                        /* True if inlist, false if outlist. */
    char inedit;                        /* Flag to indicate editing now. */
    char touched;                       /* This tool has been edited. */
    char selected;                      /* Icon selected in graph. */
    long tooltype;                      /* Sequence? Input? Branch? */
                                        /* More tool unique stuff here... */
};

#define TOUCH_EDIT              1       /* Tool has been edited. */
#define TOUCH_INIT              2       /* Tool has been initialised. */

#define TOOL_SEQUENCE           1       /* This is actually the track. */
#define TOOL_INPUT              2       /* This is an input tool. */
#define TOOL_OUTPUT             4       /* This is an output tool. */
#define TOOL_NORMAL             8       /* This is a normal tool. */
#define TOOL_ONTIME             0x10    /* This tool doesn't accept early events. */
#define TOOL_BRANCHIN           0x20    /* This tool merges two inputs. */
#define TOOL_BRANCHOUT          0x40    /* This tool has two outputs. */
#define TOOL_MACRO              0x80    /* This tool is a macro tool. */
#define TOOL_MACROOUT           0x100   /* This is the output of macro. */
#define TOOL_MACROBRANCH        0x200   /* This is the branch output of macro. */
#define TOOL_MACROIN            0x400   /* This is the input of macro. */
#define TOOL_GROUPIN            0x800   /* This tool part of group input. */
#define TOOL_MIDI               0x1000  /* This tool is MIDI capable. */
#define TOOL_NOTPAD             0x2000  /* This can not go in the toolpad. */
#define TOOL_STOOL              0x4000  /* This is a sequencer tool. */
#define TOOL_NOTPIPE            0x8000  /* This tool can not go in pipeline. */

#define MMCMD_COPYALLFILES      0x0001  /* pass the path name to copy to */
#define MMCMD_INSTALLHITLIST    0x0002  /* no parameters */
#define MMCMD_CONVERTTOHIRES    0x0003  /* no parameters */
#define MMCMD_CONVERTFROMHIRES  0x0004  /* no parameters */

struct ToolMaster {
    struct ToolMaster *next;            /* Next tool in this list. */
    long toolid;                        /* Tool ID. */
    struct Image *image;                /* Icon for this tool. */
    struct Image *upimage;              /* Icon for branching up. */
    short x,y;                          /* Position in toolbox for display. */
    char name[99];                      /* Tool name. */
    char inuse;                         /* Records instance of Tool for file io. */
    char filename[100];                 /* Where it is stored on disk. */
    struct Tool *(*createtool)();       /* Routine to allocate a new tool. */
    void (*edittool)();                 /* Routine to edit tool parameters. */
    struct Event *(*processevent)();    /* Routine to process an event. */
    struct Event *(*processlist)();     /* Routine to process a list of events. */
    void (*deletetool)();               /* Routine to delete a tool. */
    void (*removetool)();               /* Routine to close down. */
    long (*savesize)();                 /* Returns size prior to save. */
    long (*savetool)();                 /* Routine to save to disk. */
    struct Tool *(*loadtool)();         /* Routine to load from disk. */
    void (*readsysex)();                /* Pass sysex byte */
    long (*multimediafunc)();           /* passed tool ptr,(long)command,
                                           variable params. */
    void (*drawtoolicon)();             /* pass tool ptr,rastport,x,y,wid,height */
    long segment;                       /* This tool's segment list. */
    long altsegment;
    struct Track *intrack;              /* Input track for this tool. */
    short toolsize;                     /* Tool size for loading and saving. */
    char inedit;                        /* Flag to indicate editing now. */
    char selected;                      /* Icon selected in graph. */
    long tooltype;                      /* Type of tool. */
};

#define ID_MDIN 0x4D49494E
#define ID_MDOT 0x4D494F54
#define ID_MGIN 0x4D47494E
#define ID_MGOT 0x4D474F54
#define ID_PLUG 0x504C5547
#define ID_ELBO 0x454C424F
#define ID_MCOT 0x4D434F54
#define ID_MCBR 0x4D434252

struct MacroLine {
    struct Tool *toollist;              /* List of tools. */
    struct Tool *point;                 /* For making display. */
    unsigned long marks[10];            /* For figuring out display. */
    short markindex;                    /* Farthest right bit in marks. */
};

struct MacroTool {
    struct Tool tool;                   /* Tool definition. */
    struct Tool *intool;                /* Tool at start of macro. */
    struct MacroLine macroline[6];      /* Array of macrolines. */
    short inLine;                       /* Which line input goes to. */
    short toolsleft;                    /* Left edge of tool list. */
    short toolswidth;                   /* Width of tools. */
    short toolsindent;                  /* Indentation into tools. */
};

struct MacroMaster {
    struct ToolMaster master;           /* Tool Master structure. */
    struct MacroTool *macro;            /* The macro tool. */
    char saved;                         /* Latest version saved. */
};

typedef struct ToolTrayItem {
    struct ToolTrayItem *next;
    char name[30];
    struct Tool *tool;
    short x,y;
} ToolTrayItem;

typedef struct ToolTray {
    struct ToolTray *next;
    char name[16];
    ToolTrayItem *list;
    ToolTrayItem *selected;
    struct Window *window;
} ToolTray;


/*      For display purposes, mark a measure as in use. */

struct Measure {
    struct Measure *next;
    long time;
    char type;
    char inuse;
    short length;
    struct Track *track;
    short x;
    short y;
};

#define HITNAMESIZE     30

struct HitName {
    struct HitName *next;               /* Linked list. */
    unsigned char note;                 /* Equivalent MIDI value. */
    char name[HITNAMESIZE];             /* Text. */
};

typedef struct PatchNames {
    struct PatchNames *next;            /* Next in list. */
    char name[20];                      /* Name of list. */
    short usecount;                     /* # of tools using this. */
    unsigned char names[128][14];       /* Array of names. */
} PatchNames;

struct Mix {
    struct Tool ontimetool;             /* Tool to do realtime display. */
    struct NoteEvent *volumelist;       /* List of new volumes. */
    struct NoteEvent *panlist;          /* List of new pans. */
    struct NoteEvent *volumeonofflist;  /* Time stamps - when user dragged. */
    struct NoteEvent *panonofflist;     /* Pan time stamps. */
    unsigned char volume;               /* Currently displayed volume. */
    unsigned char newvolume;            /* Desired volume for display. */
    unsigned char pan;                  /* Currently displayed pan. */
    unsigned char newpan;               /* Desired pan for display. */
    unsigned char velocitydown[31];     /* Velocities of held notes. */
    unsigned char currentvolume;        /* Current volume. */
    unsigned char meter;                /* Currently displayed meter. */
    unsigned char newmeter;             /* Desired meter setting. */
    short totalvelocity;                /* Accumulated velocities. */
    char draggingvol;                   /* Volume is being dragged. */
    char draggingpan;                   /* Pan is being dragged. */
    char locked;                        /* Lock button in MixMaestro. */
    char displayed;                     /* This is displayed. */
    short leftedge;                     /* Position in display. */
    short id;                           /* Gadget ID base. */
};

struct Track {
    struct Track *next;                 /* Next Track in the list. */
    struct Edit *edit;                  /* Edit structure for this track. */
    struct Clip clip;                   /* A clip that defines the sequence. */
    struct Clip cut;                    /* A clip for global editing. */
    struct Clip undo;                   /* A clip for undo command. */
    struct EventList record;            /* Event list for recording. */
    struct Tool *toollist;              /* List of tools. */
    struct Tool tool;                   /* Sequence tool. */
    struct Tool *point;                 /* For making display. */
    unsigned char channelin;            /* MIDI Channel coming in. */
    unsigned char channelout;           /* MIDI Channel going out. */
    unsigned char mode;                 /* Mute | Through | Record. */
    unsigned char selected;             /* Track is highlighted. */
    long group;                         /* Group bits. */
    unsigned long marks[20];            /* For figuring out display. */
    short markindex;                    /* Farthest right bit in marks. */
    short height;                       /* Height in display. */
    short nameleft;                     /* Left edge of name. */
    short namewidth;                    /* Width of name field. */
    short nameindent;                   /* Indentation into name field. */
    short channelinleft;                /* Left edge of channel in. */
    short toolsinleft;                  /* Left edge of toolsin. */
    short toolsinwidth;                 /* Width of toolsin field. */
    short toolsinindent;                /* Indentation into toolsin field. */
    short playrecordleft;               /* Left edge of play/record. */
    short sequenceleft;                 /* Left edge of sequence. */
    short sequencewidth;                /* Width of sequence field. */
    long sequenceindent;                /* Indentation into sequence field. */
    short muteleft;                     /* Left edge of mute/through. */
    short toolsoutleft;                 /* Left edge of toolsout. */
    short toolsoutwidth;                /* Width of toolsout field. */
    short toolsoutindent;               /* Indentation into toolsout field. */
    short toolsouttotalwidth;           /* Total width of tools out. */
    short channeloutleft;               /* Left edge of channel out. */
    struct Measure *measures;           /* List of active measures. */
    struct Mix mix;                     /* MixMaestro stuff. */
    long bits;                          /* Various flags. */
    struct HitName *hitnamelist;        /* List of hit names. */
    PatchNames *patchnames;             /* Array of patch names. */
};

#define TB_NOUNDO       1               /* This track has no undo buffer. */
#define TB_PRINTEDIT    2               /* This track has a false edit structure. */
#define TB_STOOL        4               /* There's a sequence tool for this track. */

//Added:
#define TB_EDITMUTE 8

#define TRACK_MUTE      1
#define TRACK_THROUGH   2
#define TRACK_RECORD    4
#define TRACK_SOLOMUTE  8               /* Track was muted for solo. */
#define TRACK_INEDIT    16
#define TRACK_REALTIME  32
#define TRACK_MERGE     64              /* Merge while recording. */

// Added. -Kjetil M.
#define TRACK_LOOPED 128

struct Song {
    struct Song *next;
    long time;                          /* Start time in clocks. */
    long frametime;                     /* Start time in frames. */
    long length;                        /* Length in clocks. */
    long frameend;                      /* End time in frames. */
    struct TempoChange *changelist;     /* Tempo map for this section. */
    struct Tempo *tempolist;            /* Resulting Tempo list. */
    struct Clip masterclip;             /* Song parameters. */
    struct Section *sectionlist;        /* ABA stuff. */
    short inittempo;                    /* Initial tempo. */
    short id;
    char name[100];
    short color;                        /* Display color. */
    short y;                            /* Display coordinates. */
};

struct Accessory {
    struct Accessory *next;             /* Next accessory in this list. */
    long id;                            /* Accessory ID. */
    struct Image *image;                /* Icon for this accessory. */
    struct Image *onimage;              /* Icon for when selected. */
    char name[100];                     /* Name. */
    char filename[100];                 /* Where it is stored on disk. */
    struct Window *window;
    unsigned short left,top;            /* Position of edit window. */
    unsigned short width,height;        /* Size of edit window. */
    unsigned short x,y;                 /* Position in access box. */
    long (*remove)();
    long (*edit)();                     /* Routine to edit accessory. */
    long (*open)();                     /* Routine to open it. */
    long (*close)();
    long (*size)();
    long (*save)();
    long (*load)();
    long (*install)();                  /* Install in environment. */
    long (*clear)();                    /* Clear from environment. */
    long (*expandc)();                  /* Future routine? */
    long segment;                       /* This accessory's segment list. */
    long altsegment;                    /* Alternate segment list. */
    char selected;                      /* Icon selected in graph. */
};

#define TC_START        1
#define TC_STOP         2
#define TC_POSITION     3
#define TC_RECORDON     4
#define TC_RECORDOFF    5
#define TC_PLAY         6
#define TC_TICK         7

struct Transport {
    struct Transport *next;
    void (*routine)();
    char priority;
    char loopflag; /* set if used loop transport routine */
    char pad[2];
    long offset;
};


/*      There is a linked list of Tempo nodes.  Each specifies a clock cycle,
        buffer size, and repeat count.  The last cycles forever.
*/

struct Tempo {
    struct Tempo *next;                 /* Next Tempo in list. */
    long time;                          /* Time, in clocks, this starts. */
    unsigned short cycle;               /* Sample rate. */
    unsigned short bpm;                 /* Beats per minute. */
    long repeat;                        /* Repeat count. */
    unsigned long frame;                /* Frame this starts on. */
    long offset;                                /* offset for repeats */
};

/*      For editing the Tempo, the user sets Tempo Changes. */

struct TempoChange {
    struct TempoChange *next;
    long time;                          /* When this occurs, in clicks. */
    char eventtype;                     /* What type of event. */
    char type;                          /* Type of change. */
    short bpm;                          /* Beats per minute. */
    long duration;                      /* How long the change should take. */
    struct Tempo *tempos;               /* Pointer into list of tempos. */
};

#define TC_INSTANT      1
#define TC_LINEAR       2
#define TC_EXPCURVE     3
#define TC_LOGCURVE     4

struct SMPTE {
    unsigned char hours, minutes, seconds, frames;
};

struct Section {
    struct Section *next;
    long time;
    char type;                          /* What type of event. */
    char pad[3];        /* in use in song.c, aba.c */
    struct String *string;              /* Pointer to string. */
    long length;                        /* Length of this section. */

//Added from bars.h: -Kjetil M:
    char pattern;
    unsigned char repeat_count;         /* how many times to repeat (255 is infinite) */
    unsigned char current_repeat;       /* tracks the current repeat count */
    char c_more;        /* room for more */
};


/*      Print flags */

#define PRINT_SECTIONS          1
#define PRINT_BARNUMBERS        2
#define PRINT_TEMPO             4
#define PRINT_CHORDS            8
#define PRINT_LYRICS            0x10
#define PRINT_DYNAMICS          0x20
#define PRINT_CONCERT           0x40
#define PRINT_TRANSPOSED        0x80
#define PRINT_HIRES             0x100
#define PRINT_BLOWUP            0x200

/*      Flag Bits. */

#define FLAGS_TWOPLANES         1               /* Two colors instead of three. */
#define FLAGS_SIMPLEREFRESH     2               /* Simple refresh windows. */
#define FLAGS_DEMOMODE          4               /* Currently in demo mode. */
#define FLAGS_COMBINETEMPO      8               /* Combine Timeline tempo maps. */
#define FLAGS_COMBINESECTIONS   0x10            /* Combine Timeline section lists. */
#define FLAGS_COMBINEPARAMS     0x20            /* Combine Timeline song params. */
#define FLAGS_DISABLERAWKEYS    0x40            /* Disable rawkeys for menu rawkeys */
#define FLAGS_SAVEICONS         0x80            /* Save file icons. */
#define FLAGS_WINDOWTOFRONT     0x100           /* Automatic Window to front. */
#define FLAGS_DOFLASH           0x200           /* Exproding windows. */
#define FLAGS_WBPOINTER         0x400           /* Use Workbench pointer. */
#define FLAGS_LACE              0x800           /* Run in interlace. */
#define FLAGS_UNDOBUFF          0x1000          /* Enable undo buffer. */
#define FLAGS_CLOSEWB           0x2000          /* Close WorkBench. */
#define FLAGS_USEASL            0x4000          /* use 2.0 asl requester */
#define FLAGS_DOUBLEWIDTH       0x8000          /* 2.0 double width screen. */
#define FLAGS_DOUBLEHEIGHT      0x10000         /* 2.0 double height screen. */
#define FLAGS_WBDISPLAY         0x20000         /* 2.0 copy workbench display. */

/* Added -Kjetil M. */
#define FLAGS_DONTLOADTOOLNAMES 0x40000
#define FLAGS_68000             0x80000
#define FLAGS_CONTINUEATRIGHT   0x100000

/* Added A.Faust 01-Feb-2001, user have choosen his own Display  */
#define FLAGS_USERDISPLAY       0x200000

/*      Save flags. (long functions->saveflags)*/

#define SFLAG_TOOLNAMES         1               /* Display tools with names. */
#define SFLAG_LOCKTOMAP         2               /* Lock to tempo map. */
#define SFLAG_SMPTEOFFSET       4               /* Display SMPTE offset. */
#define SFLAG_MIDIDEFAULTS      8               /* Transmit MIDI defaults */
#define SFLAG_PIANOAUTORANGE    16              /* piano roll auto range on/off */

/* Added -Kjetil M. */
#define SFLAG_DONTALPHABETIZE   32              /* alphabetize tools */ /* From bars.h */
#define SFLAG_MIXAUDITION       64

/* Metro flags. Added, -Kjetil M. */
#define METRO_COUNTDOWN     0x7F  //(1<<1)
#define METRO_LEADINONLY    0x80  //(1<<3)
#define METRO_MEASUREMASK   0x3F  //(1<<2)

/*      Array of pointers to shared data, functions and library pointers,
        so all tools can share these. */

#define RES_MEASURE             0
#define RES_BEAT                1
#define RES_ANYWHERE            2
#define RES_SECOND              3
#define RES_FRAME               4

typedef struct ToolName {
    struct ToolName *next;
    long toolid;
    char name[40];                      /* Name. */
    char filename[100];                 /* Where it is stored on disk. */
} ToolName;

struct Functions {
    char locked;
    char measureres;                    /* Cuts resolve to measures. */
    char recording;                     /* Set when recording. */
    char running;                       /* Set when running. */
    char punchenabled;                  /* Auto punch in and out enabled. */
    char loopenabled;                   /* Loop mode turned on. */
    char clicking;                      /* Click track on? */
    char seeclick;                      /* Visual metronome. */
    char multiin;                       /* Multiple inputs? */
    char clickchannel;                  /* MIDI CLick channel. */
    char midiclock;                     /* Sync to MIDI clocks. */
    char smpteclock;                    /* Sync to SMPTE. */
    char sendmidiclock;                 /* Send out MIDI clocks. */
    char smptetype;                     /* Which SMPTE format. */
    char countdown;                     /* Do count down. */
    char midiclick;                     /* Use MIDI for click track. */
    char chop;
    long leadinstart;                   /* Length of lead in. */
    long timenow;                       /* Current time. */
    unsigned long markone;              /* Auto locate register. */
    unsigned long marktwo;              /* Auto locate register. */
    unsigned long punchin;              /* Punch in point. */
    unsigned long punchout;             /* Punch out point. */
    unsigned long loopin;               /* Loop in point. */
    unsigned long loopout;              /* Loop out point. */
    unsigned long cutin;                /* Cut in point. */
    unsigned long cutout;               /* Cut out point. */
    long starttime;                     /* Where to play from. */
    long stoptime;                      /* Marker to stop at. */
    unsigned long padcutin;             /* For non real time edits. */
    unsigned long padcutout;            /* For non real time edits. */
    unsigned long songlength;
    long startoffset;                   /* Starting hi res clock offset. */
    unsigned short tempos[4];
    unsigned short tempo;               /* Current tempo. */
    unsigned short inittempo;           /* Initial tempo. */
    char songname[100];
    char author[100];
    short palette[8];                   /* Colors. */
    char remotecontrol[128];            /* Table of remote controls. */
    unsigned long markthree;            /* Auto locate register. */
    unsigned long markfour;             /* Auto locate register. */
    unsigned char volumenum;            /* Mix Maestro Volume CC. */
    unsigned char pannum;               /* Mix Maestro Pan CC. */
    unsigned char subdivide;            /* Metronome subdivision. */
    char bypassmix;                     /* Use undo buffer. */
    long savestop;                      /* Place to save stop sign. */
    long printflags;                    /* Print flags. */
    unsigned long recordfilters;
    long userlength;                    /* User defined song length. */
    long saveflags;
    char clickvolume;                   /* metronome volume */

//    char cmore[3];                    /* Expansion space. */
// from environ.h:
    char cmore;                         /* Expansion space. */
    unsigned char measures_per_line;    /* printing */
    unsigned char staves_per_page;      /* printing */
    //unsigned char note_spacing;         /* printing */

//    long more[3];                     /* Expansion space. */
// from bars.h: -Kjetil M.
    long more;                          /* Expansion space. */
    unsigned short printbuf_x, printbuf_y;  /* size of print buffer */
    long time_offset;                   /* current time offset taking into regard repeats */

    struct Tool *midiouttool;           /* Tool to send MIDI Metronome. */
    struct Tool *clockouttool;          /* Tool to send MIDI Clocks. */
    ToolTray *tooltraylist;             /* List of tool trays. */
    PatchNames *patchlist;              /* List of patch names lists. */
    struct Song *songlist;              /* For time-line stuff. */
    unsigned long clockcycles;          /* Time in clock cycles! */
    char useclip;                       /* Use the ClipBoard. */
    char externclock;                   /* External clock flag. */
    short timeroffset;                  /* For external clock. */
    struct Event *coordlist;            /* Window coordinates. */
    struct Track *tracklist;            /* Top track in list. */
    struct Clip masterclip;             /* Master key, chord, signature. */
    struct Clip masterundo;
    struct Clip mastercut;
    struct Edit *masteredit;
    struct Tool *edittools[16];         /* 16 Tools to edit with. */
    unsigned short toolid;              /* Global tool id. */
    short groupid;                      /* Currently selected group. */
    struct Chord *scalelist;            /* All scales. */
    struct Chord *chordlist;            /* All chords. */
    struct Rhythm *rhythmlist;          /* All rhythms. */
    struct Section *sectionlist;        /* ABA list. */
    struct TempoChange *tempochangelist;/* Tempo Change list. */

    long whoops;                        /* Time line song list used to be here. */
    long frame;                         /* Current frame. */
    unsigned long hirestime;            /* Hi res clock. */
    char version;
    long SysBase;                       /* Exec library. */
    long DOSBase;                       /* DOS library. */
    long IntuitionBase;
    long GfxBase;
    long LayersBase;
    long standardout;                   /* For printf. */
    struct Event *pipequeue;            /* Event queue. */
    struct Event *earlyqueue;           /* Ahead of time queue. */
    struct Screen *screen;              /* Screen we all exist in. */
    struct Window *window;              /* Main window. */
    struct ToolMaster *toolmasterlist;  /* All ToolMasters. */
    struct Accessory *accesslist;       /* All Accessories. */
    struct Tool *wasmidiouttool;        /* Tool to send MIDI Metronome. */
    long (*stealmidi)();                /* Steal serial interrupt. */
    long (*releasemidi)();              /* Release MIDI. */

/*      New additions. */

    long (*audioclick)();               /* Play click sound. */

    struct Track *selectedtrack;        /* Track currently selected. */

    long flags;                         /* Additional flags. */
    char *filename;                     /* Pointer to filename buffer. */
    long sunrizeflags;
    struct DisplayFlags *displayflags;  /* Display flags for updateobject. */
    short displaywidth, displayheight;
    char osbits;                        /* Used by osms output. */
    char mmrecord;                      /* Enable mm record. */
    unsigned short CAMD_count;          /* count of open CAMD tools/accessories */
    struct MidiNode *CAMD_Node;         /* midi node for CAMD */
    char *bppdir;                       /* directory&path BPP was run from */
    long *lockdir;                      /* Lock to bppdir */

//    long pad[48];                     /* Room for more. */
    struct Window *  windowtofront;     /* if this has a value, main.c brings it to front */
    long             pad[39];           /* Room for more. */

    int (* HandleRopeGadget)(struct Window *win,struct Gadget *gad,
                             short mouse_x, short mouse_y);
    void    (* SetRopeInfo)(struct Window *win,short gadnum,char *text);
    void (* InitRopeGadget)(struct Window *win,struct Gadget *gad,
                            short next_gad,short prev_gad);
    void (* FreeRopeGadget)(struct Gadget *gad);

    /* installtransportpl provides an extra offset value for repeating/
     * looping purposes
     */

    struct Transport *(* installtransportpl)(void(* routine)(void),char priority);


/* NewScrollingPopUpMenu provides ptr2 and index, which allow the
 * pop up requester to start in the middle instead of always at the
 * top
 */
   struct ListNode *(* NewScrollingPopUpMenu)(struct Screen *screen,
                     void *ptr1, void (* displayroutine)(void),
                     short itemheight, short itemwidth, short count,
                     void *ptr2, short index);

/* doubleclick returns a non-zero amount of time if a doubleclick occurs */
   int (* doubleclick)(struct IntuiMessage *message);

/* msgbox is a routine to put a requester on the screen with one to
 * three buttons. It returns the number of button from 0 to 2 from the left
 */
    int (* msgbox) (char *text,char *b0Text,char *b1Text,
                   char *b2Text,short center,short backcolor,short textcolor,
                   short buttoncolor);


    void (*calctrack)(struct Track *track);
    void (*removealltool)(struct ToolMaster *toolmaster,char remove);
    void (*copyfile)(char *fromname,char *topath);

    long (*removetoolfrommacro)(struct MacroTool *macro,
                struct ToolMaster *toolmaster,
                char remove);
    void (*edittool)(struct Tool *tool);
    void (*addtoolremover)(long (*routine)());
    void (*removetoolremover)(long (*routine)());

    void (*drawslice)(struct RastPort *rp,struct Image *image,
        short x,short y,short start,short finish);
    void (*drawtool)(struct Tool *tool,
        struct RastPort *rp,short x,short y,short h);
    long (*checklegaltool)(char *id);   /* for MM tools, check if legal */

    void (*settimenow)(long time);      /* Set current time. */
    void (*updateobject)(void *object,void *data);
    void (*linkobject)(struct Window *window,void *object,void (*routine)());
    void (*unlinkobject)(struct Window *window,void *object);
    void (*unlinkwindow)(struct Window *window);

    void (*installhitlist)(struct Track *track,struct HitName *list);

    void (*settempo)(short tempo);
    void (*installtempochangelist)(struct TempoChange *);

    struct Transport *(*installtransportp)(void (*routine)(void),
        char priority);

    long (*fastgets)(long,long,long);   /* fgets() */
    long (*fastseek)(long,long,long);   /* Fast seek file io. */

    void (*adddisplayserver)(void (*routine)(void),
                      struct Window *window);
    void (*removedisplayserver)(struct Window *window);

    struct Tool *(*dragtool)(struct Window *window,
                      struct ToolMaster *toolmaster,
                      struct Tool *copy);
    void (*addtoolserver)(struct Tool * (*routine)(void),
                   struct Window *window);
    void (*removetoolserver)(struct Window *window);

    long (*editsysex)(struct Track *track,
              struct StringEvent *stringevent);
    struct Event *(*dupeevent)();       /* Duplicate an event. */

    long (*frametotime)(long frame);    /* Convert frame to time. */
    long (*timetoframe)(long time);     /* Convert time to frame. */
    long (*frametohmsf)(long frame);    /* Convert frame to hmsf. */
    long (*hirestotime)(long hires);    /* Hires clock to time. */
    long (*timetohires)(long time);     /* Time to hires. */
    long (*hirestoframe)(long hires);   /* Hires clock to frame. */
    long (*frametohires)(long frame);   /* Frame to hires clock. */

    void (*deletetool)(struct Tool *tool);
    struct Tool *(*createtool)(struct ToolMaster *toolmaster,
        struct Tool *copy);
    long (*sizetool)(struct Tool *tool);
    long (*savetool)(long file,
              struct Tool *tool);
    struct Tool *(*loadtool)(long file,
        struct ToolMaster *toolmaster,
        long size);
    struct ToolMaster *(*gettoolmaster)(long id);

/*      Old routines: */

    void (*processsmpteclock)(struct Event *event);
    void (*processmidiclock)(struct Event *event);/* Process a MIDI clock event .*/
    void (*processsysex)();             /* Does nothing. */
    void (*processinputevent)(char command);

    long (*clearenvironment)(struct Environment *environ,
        char all);
    long (*installenvironment)(struct Environment *environ);
    long (*loadsong)(long file,struct Environment *environ);
    long (*savesong)(long file,struct Environment *environ);

    struct Transport *(*installtransport)(void (*routine)(void));
    void (*removetransport)(void (*routine)(void));/* Remove transport handler. */
    void (*transportcommand)(unsigned char command,
        long time);             /* Send command to handlers. */

    void (*qevent)(struct Event *event);/* Put event in queue. */
    struct Event *(*allocevent)();      /* Allocate an event. */
    struct Event *(*fastallocevent)();  /* Allocate an event from interrupt. */
    void (*freeevent)(struct Event *event);/* Free an event. */
    struct Event *(*sorteventlist)(struct Event *events);
    void (*freelist)(struct Event *list);/* Free a list of events. */
    struct Event *(*dupelist)(struct Event *list); /* Duplicate a list. */

    struct String *(*allocstring)(char *text);
    void (*freestring)(struct String *string);
    void (*replacestring)(struct String **string,
                   char *text);
    void (*dupestring)(struct String **string);
    char *(*stringtext)(struct String *string);

    void (*clearclip)(struct Clip *clip);/* Clear a clip. */
    void (*dupeclip)(struct Clip *source,
              struct Clip *destination);
    void (*cutclip)(struct Clip *source,
        struct Clip *clip,
        unsigned long begin,
        unsigned long end);
    void (*copyclip)(struct Clip *source,
              struct Clip *clip,
              unsigned long begin,
              unsigned long end);
    void (*pasteclip)(struct Clip *source,
               struct Clip *clip,
               unsigned long begin);
    void (*mixclip)(struct Clip *source,
             struct Clip *clip,
             unsigned long begin,
             unsigned long end);
    struct Clip *(*loadclip)(long filein);
    long (*saveclip)(long filein,
             struct Clip *clip);
    long (*clipboard)(long command,
              struct Clip *clip);

    struct Track *(*createtrack)(char new,
        char in);
    void (*deletetrack)(struct Track *track);   /* Delete a track. */

    char *(*myalloc)(long size,long flags);     /* Internal allocation routine. */
    void (*myfree)(char *item,long size);       /* Internal free routine. */

    long (*doscall)();                  /* Make a DOS command. */
    long (*fastopen)(char *name,long mode);     /* Fast file open. */
    long (*fastwrite)(long file,char *buffer,long size);
    long (*fastread)(long file,char *buffer,long size);
    long (*fastclose)(long file);       /* Fast file close. */

    long (*popupkey)(struct Window *window,long was);
    long (*popupnote)(struct Window *window,long was);
    long (*popupoctave)(struct Window *window,long was);
    struct Chord *(*popupchord)(struct Chord *old);
    struct Rhythm *(*popuprhythm)(struct Rhythm *old);
    struct Chord *(*popupscale)(struct Chord *old);

    long (*measuretotime)(struct Clip *clip,unsigned short measure);
    long (*timetomeasure)(struct Clip *clip,long time);
    long (*totalbeatstotime)(struct Clip *clip,unsigned long beats);
    long (*timetototalbeats)(struct Clip *clip,unsigned long time);
    void (*lengthtostring)(struct Clip *clip,
                    unsigned long time,
                    unsigned long length,
                    char *buffer);
    unsigned long (*stringtolength)(struct Clip *clip,
                             unsigned long time,
                             char *buffer,
                             unsigned long length);
    void (*timetostring)(struct Clip *clip,
        long time,
        char *buffer);
    unsigned long (*stringtotime)(struct Clip *clip,
        char *buffer,
        unsigned long time);
    void (*frametostring)(long frame,
        char *buffer);
    unsigned long (*stringtoframe)(char *buffer,
        unsigned long frame);
    void (*notetostring)(struct Clip *clip,
        unsigned short note,
        char *string);
    short (*stringtonote)(char *string);

    long (*noteinkey)(struct Clip *clip,
              struct NoteEvent *note);
    long (*noteinchord)(struct Clip *clip,
              struct NoteEvent *note);
    long (*noteinrhythm)(struct Clip *clip,
              struct NoteEvent *note);
    struct KeyEvent *(*timetokey)(struct Clip *clip,
        unsigned long time);
    struct ChordEvent *(*timetochord)(struct Clip *clip,
        unsigned long time);
    unsigned short (*timetodynamics)(struct Clip *clip,
        long time);
    struct NoteEvent *(*nextrhythmbeat)(struct Clip *clip,
        long time,
        long *eventtime);
    long (*scaletotwelve)(struct Clip *clip,
        unsigned long time,
        char value);
    long (*twelvetoscale)(struct Clip *clip,
        unsigned long time,
        char value,
        char *offset);
    long (*random)(long range);         /* Return random number. */

    long (*areyousure)(char *text);     /* Put up "are you sure?" requester. */
    void (*openwait)(void);             /* Open wait requester. */
    void (*closewait)(void);            /* Close wait requester. */

    void (*display)(long refresh);      /* Display main window. */
    struct ListNode *(*ScrollingPopUpMenu)(struct Screen *screen,
        struct ListNode *list,
        void (*displayroutine)(void),
        short itemheight,
        short itemwidth,
        short count);
    long (*DragSlider)(struct Window *window,
        struct Gadget *gadget,
        long width,
        /* void */ long *routine, //altered by A.Faust
        char axis);
    void (*DrawSlider)(struct Window *window,
        struct Gadget *gadget,
        long position,
        long width,
        /* void */long *routine,
        char axis);
/*      Inovatools 1 routines. */
    long (*Itoa)(long i,char *s,long r);
    long (*Atoi)(char *s,long r);
    void (*RefreshGadget)(struct Window *window,
        long id);
    struct Gadget *(*GetGadget)(struct Window *window,
        unsigned short id);
    struct StringInfo *(*GetStringInfo)(struct Window *window,
        unsigned short id);
    void (*SetStringInfo)(struct Window *window,
        unsigned short id,
        char *string);
    long (*GetStringInfoNumber)(struct Window *window,
        unsigned short id,
        unsigned short base);
    void (*SetStringInfoNumber)(struct Window *window,
        unsigned short id,
        long number,
        long base);
    struct PropInfo *(*GetPropInfo)(struct Window *window,
        unsigned short id);
    void (*SetPropInfo)(struct Window *window,
        unsigned short id,
        unsigned short hpos,
        unsigned short vpos);
    struct IntuiMessage *(*GetSelectIntuiMessage)(
        struct Window *window,
        unsigned long type);
    struct IntuiMessage *(*GetIntuiMessage)(struct Window *window);
    void (*ClearIntuiMessages)(struct Window *window);
    struct Menu *(*GetMenu)(
        struct Menu *menulist,
        long menunum);
    struct MenuItem *(*GetMenuItem)(
        struct Menu *menu,
        long menunum,
        long itemnum,
        long subitemnum);
    void (*CheckMenuItem)(
        struct Menu *menu,
        long menunum,
        long itemnum,
        long subitemnum,
        long on);
    void (*EnableMenuItem)(
        struct Menu *menu,
        long menunum,
        long itemnum,
        long subitemnum,
        long on);
    void (*EnableMenu)(
        struct Menu *menu,
        long menunum,
        long on);
    void (*EnableGadget)(
        struct Window *window,
        short id,
        short on);
    void (*SelectGadget)(
        struct Window *window,
        short id,
        short on);
    struct NewWindow *(*DupeNewWindow)(struct NewWindow *nw);
    void (*DeleteNewWindow)(struct NewWindow *nw);
    struct Menu *(*DupeMenu)(struct Menu *source);
    void (*DeleteMenu)(struct Menu *list);
    void (*SendCloseWindow)(struct Window *window);
    long (*List_Len)();
    long (*List_Position)();
    struct ListItem *(*List_Pred)();
    struct ListItem *(*List_Index)();
    struct ListItem *(*List_Cat)();
    struct ListItem *(*List_Insert)();
    struct ListItem *(*List_Remove)();
    void (*SetScrollBar)(struct ListInfo *info);
    void (*DrawList)(struct ListInfo *info);
    void (*ScrollList)(struct ListInfo *info);
    struct ListItem *(*SizeList)(struct ListInfo *li);
    struct ListItem *(*InitListInfo)(struct ListInfo *li);
    void (*RemoveListInfo)(struct ListInfo *li);
    struct ListItem *(*GetListItem)(struct ListInfo *li);
    void (*ClickList)(struct ListInfo *li,short id);
    void (*InsertListItem)(struct ListInfo *li,struct ListItem *item);
    struct ListItem *(*RemoveListItem)(struct ListInfo *li);
    short (*PopUpMenu)(struct Window *window,struct Menu *menu);
    void (*DragGadget)(struct Window *window,struct DragInfo *g);
    void (*FileName)(
        char *blankstring,
        char *prompt,
        char *ptemplate,
        struct Screen *screen,
        short mode,
        short x,
        short y);
    struct Window *(*FlashyOpenWindow)(struct NewWindow *new);
    void (*FlashyCloseWindow)(struct Window *window);
    struct Window *(*WhichWindow)(struct Window *window,
        short x,
        short y);
    struct ListInfo *(*DupeListInfo)(struct ListInfo *listinfo);
    void (*DeleteListInfo)(struct ListInfo *listinfo);
    void (*RealTimeScroll)(struct ListInfo *li);
    void (*EmbossOn)(struct Window *,short ,short );
    void (*EmbossOff)(struct Window *,short );
    void (*DrawEmbossed)(struct Window *,short );
    void (*EnableEmbossed)(struct Window *,short ,short );
    void (*SelectEmbossed)(struct Window *,short ,short );
    void (*ModifyEmbossedProp)(struct Window *,short,long,long,long,long,long,long);
    long (*DragEmbossedProp)(struct Window *,short );
    long (*DrawEmbossedProp)(struct Window *,short );
    long (*ShiftEmbossedPropOnce)(struct Window *,short,short,short );
    long (*ShiftEmbossedProp)(struct Window *,short,short ,short );
    long (*EmbossedPropOn)(struct Window *,short,long (*)(void),unsigned short ,
         unsigned short );
    void (*FatEmbossedPropOn)(struct Window *,short ,short ,short ,
        long (*)(void),unsigned short ,char );
    void (*EmbossedPropOff)(struct Window *,short );
    void (*FatEmbossedPropOff)(struct Window *,short ,short ,short );
    void (*DrawEmbossedRect)(struct RastPort *,short ,short ,short ,short ,short );
    void (*DrawEmbossedWindow)(struct Window *,short );
    void (*EmbossWindowOff)(struct Window *);
    void (*EmbossWindowOn)(struct Window *,
        long ,char *,unsigned short ,unsigned short ,long (*)(void),long (*)(void));
    void (*EmbossRequestOn)(struct Window *,char *);
    void (*RefreshEmbossedWindowFrame)(struct Window *);
    int (*SystemGadgets)(struct Window *,long ,struct Gadget *,long);
    void (*ModifyWindowProps)(struct Window *,unsigned long ,unsigned long ,
        unsigned long ,unsigned long ,unsigned long ,unsigned long );
    void (*EmbossedWindowTitle)(struct Window *,char *);
    void (*EmbossedWindowText)(struct Window *,char *,short );
    void (*setsmalltext)(struct Window *window,short id,char *name);
    short (*fitsmalltext)(char *text,short length);
    void (*usetopazfont)(struct RastPort *rp);
    void (*usesmallfont)(struct RastPort *rp);
};

/* bits for "globes" assigned in main.c */
#define GLOBES_USEASLREQUEST 1          /* use 2.0 asl requester */
#define GLOBES_INTUITION37      2               /* 2.0 intuition */

/* macros for saving directory locations */
#define FILENAME_TOOL   0
#define FILENAME_ACCESS 1
#define FILENAME_SONG   2
#define FILENAME_TRACK  3
#define FILENAME_GROUP  4
#define FILENAME_TEMPO  5
#define FILENAME_READ   6
#define FILENAME_WRITE  7

#ifndef FILENAME_MAX
#define FILENAME_MAX    8
#endif

/* added A.Faust - adapted from R.Hagens "myheader.h" */

typedef int (* int_cast)();
typedef long (* long_cast)();
typedef ULONG (* ulong_cast)();
typedef long (* short_cast)();
typedef void (* void_cast)();
typedef struct NoteEvent *(* NoteEvent_cast)();
typedef struct Tool *(* Tool_cast)();
typedef struct Event *(*event_cast)();
typedef struct ListItem *(*List_cast)();

/* that is a store for the pageprint opts */

struct PrintOpt{

    struct PrintOpt *next;
    int measuresperline;
    int stavesperpage;
    int notespacing;
};

//endadd


#endif
