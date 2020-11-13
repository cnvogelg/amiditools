#ifndef TOOLS_H
#define TOOLS_H


/*      INOVATOOLS 1 header file.
        (c) 1987 Todor Fay
*/

/*      Knob Flags */

#define KNOB_FILL       1l      /* When drawing, fill knob. */
#define KNOB_DRAW       2l      /* Draw this knob. */
#define KNOB_RANGED     4l      /* This knob is not full circle. */
#define KNOB_COMPLEMENT 8l      /* Draw the arrow in COMPLEMENT mode. */
#define KNOB_CALCVALUE  16l     /* Calculate a value from the Angle, Range, 
                                   StartAngle, and StopAngle. */
#define KNOB_MARKENDS   32l     /* Mark begin and end of sweep. */
#define KNOB_MARKED     64l     /* Mark all step increments in knob. */

struct Knob {
    struct Knob *NextKnob;      /* Pointer to next knob in list. */
    USHORT XCenter, YCenter;    /* Center of Knob. */
    USHORT Flags;               /* KNOB_FILL, KNOB_DRAW, etc. */
    USHORT Value;               /* Calculated value. */
    USHORT Range;               /* Range for calculated value. */
    USHORT Angle;               /* Current angle (0 - 65534) */
    USHORT Offset;              /* Initial Offset Angle. */
    USHORT Steps;               /* Number of steps to increment. */
    USHORT ArrowRadius;         /* Arrow's radius. */
    USHORT KnobRadius;          /* Knobs's radius. */
    USHORT StartAngle;          /* Starting point in knob swwep. */
    USHORT StopAngle;           /* Finishing point. */
    USHORT FillColor;           /* Color to Fill the knob. */
    USHORT BorderColor;         /* Color for edge of knob. */
    USHORT ArrowColor;          /* Color for arrow. */
    USHORT KnobID;              /* User id. */
    USHORT AX, AY, OX, OY, OS;  /* Leave these alone. */
    void (*UpdateRoutine) ();   /* User supplied routine. */
    long UserData;              /* User supplied data. */
};

/*      Flags for DragInfo */

#define DRAG_INWINDOW   1l      /* Only draw in Window's RastPort */
#define DRAG_OUTLINE    2l      /* Pen 0 is transparent. */
#define DRAG_MOVEGADGET 4l      /* Leave gadget at new position. */

struct DragInfo {
    struct Gadget *Gadget;      /* Gadget being dragged. */
    SHORT LeftEdge, TopEdge, RightEdge, BottomEdge;  /* Bounding box. */
    USHORT Flags;               /* DRAG_INWINDOW, etc... */
    SHORT XPos, YPos;           /* Current Position in window. */
    void (*UpdateRoutine)();   /* User supplied routine. */
    LONG DD;                    /* Leave this alone. */
};

struct ListItem {
    struct ListItem *Next;
};

struct ListInfo {
    USHORT LeftEdge, TopEdge, Width, Height;
    USHORT ItemHeight;
    USHORT ScrollID, ListID, ClickUpID, ClickDownID;
    USHORT BorderPen, FillPen;
    struct ListItem *TopItem, *TopDisplayItem, *ActiveItem;
    struct Window *Window;
    struct Gadget *ScrollGadget, *ListGadget, *ClickUpGadget, *ClickDownGadget;
    void (*DrawRoutine) ();
    USHORT OldL, OldT, OldW, OldH;
};

#define FILES_DELETE    1l
#define FILES_OPEN      2l
#define FILES_SAVE      4l
#define FILES_TEST      8l
#define FILES_TYPE      16l
#define FILES_PATH      32l
#define FILES_SAVEDEMO 64l
#define FILES_DUMMYEXT 128l

#if 0
long Itoa(long, char *, long);
long Atoi(char *, long);
void RefreshGadget(struct Window *,long);
struct Knob *GetKnob(struct Knob *,long);
struct Gadget *GetGadget(struct Window *,long);
struct StringInfo *GetStringInfo(struct Window *,long);
void SetStringInfo(struct Window *,long,char *);
long GetStringInfoNumber(struct Window *,long,long);
void SetStringInfoNumber(struct Window *,long,long,long);
struct PropInfo *GetPropInfo(struct Window *,long);
void SetPropInfo(struct Window *,long,long,long);
struct IntuiMessage *GetSelectIntuiMessage(struct Window *,long);
struct IntuiMessage *GetIntuiMessage(struct Window *);
void ClearIntuiMessages(struct Window *);
struct Menu *GetMenu(struct Menu *,long);
struct MenuItem *GetMenuItem(struct Menu *,long,long,long);
void CheckMenuItem(struct Menu *,long,long,long,long);
void EnableMenuItem(struct Menu *,long,long,long,long);
void EnableMenu(struct Menu *,long,long);
void EnableGadget(struct Window *,long,long);
void SelectGadget(struct Window *,long,long);
struct NewWindow *DupeNewWindow(struct NewWindow *);
void DeleteNewWindow(struct NewWindow *);
struct Knob *DupeKnobList(struct Knob *);
void DeleteknobList(struct Knob *);
struct Menu *DupeMenu(struct Menu *);
void DeleteMenu(struct Menu *);
void SendCloseWindow(struct Window *);
struct Knob *KnobGadgets(struct Window *,struct Knob *);
void DrawArrow(struct RastPort *,struct Knob *);
void DrawKnobs(struct Window *,struct Knob *);
long List_Len(struct ListItem *);
long List_Position(struct ListItem *,struct ListItem *);
struct ListItem *List_Pred(struct ListItem *,struct ListItem *);
struct ListItem *List_Index(struct ListItem *,long);
struct ListItem *List_Cat(struct ListItem *,struct ListItem *);
struct ListItem *List_Insert(struct ListItem *,struct ListItem *,long);
struct ListItem *List_Remove(struct ListItem *,struct ListItem *);
void SetScrollBar(struct ListInfo *);
void DrawList(struct ListInfo *);
void ScrollList(struct ListInfo *);
struct ListItem *SizeList(struct ListInfo *);
struct ListItem *InitListInfo(struct ListInfo *);
void RemoveListInfo(struct ListInfo *);
struct ListItem *GetListItem(struct ListInfo *);
void ClickList(struct ListInfo *,long);
void InsertListItem(struct ListInfo *,struct ListItem *);
struct ListItem *RemoveListItem(struct ListInfo *);
long PopUpMenu(struct Window *,struct Menu *);
void DragGadget(struct Window *,struct DragInfo *);
void FileName(char *,char *,char *,struct Screen *,long,long,long);
void EditColors(struct Screen *,long,long,long);
struct Window *FlashyOpenWindow(struct NewWindow *);
void FlashyCloseWindow(struct Window *);
struct Window *WhichWindow(struct Window *,long,long);
struct ListInfo *DupeListInfo(struct ListInfo *);
void DeleteListInfo(struct ListInfo *);
#endif
#endif //TOOLS_H
