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

#include <intuition/intuition.h>

static struct IntuiText midiinIText1 = {
	4,2,JAM1,
	6,1,
	NULL,
	"System Exclusive",
	NULL
};

static struct Gadget midiinGadget7 = {
	NULL,
	8,81,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText1,
	0,
	NULL,
	7,
	NULL
};

static struct IntuiText midiinIText2 = {
	4,2,JAM2,
	13,1,
	NULL,
	"Program Change",
	NULL
};

static struct Gadget midiinGadget6 = {
	&midiinGadget7,
	8,70,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText2,
	0,
	NULL,
	4,
	NULL
};

static struct IntuiText midiinIText3 = {
	4,2,JAM2,
	7,1,
	NULL,
	"Mono After-Touch",
	NULL
};

static struct Gadget midiinGadget5 = {
	&midiinGadget6,
	8,37,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText3,
	0,
	NULL,
	5,
	NULL
};

static struct IntuiText midiinIText4 = {
	4,2,JAM2,
	7,1,
	NULL,
	"Poly After-Touch",
	NULL
};

static struct Gadget midiinGadget4 = {
	&midiinGadget5,
	8,48,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText4,
	0,
	NULL,
	2,
	NULL
};

static struct IntuiText midiinIText5 = {
	4,2,JAM2,
	13,1,
	NULL,
	"Control Change",
	NULL
};

static struct Gadget midiinGadget3 = {
	&midiinGadget4,
	8,59,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText5,
	0,
	NULL,
	3,
	NULL
};

static struct IntuiText midiinIText6 = {
	4,2,JAM2,
	24,1,
	NULL,
	"Pitch Wheel",
	NULL
};

static struct Gadget midiinGadget2 = {
	&midiinGadget3,
	8,26,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText6,
	0,
	NULL,
	6,
	NULL
};

static struct IntuiText midiinIText7 = {
	4,2,JAM2,
	24,1,
	NULL,
	"Note On/Off",
	NULL
};

static struct Gadget midiinGadget1 = {
	&midiinGadget2,
	8,15,
	139,9,
	GADGHBOX+GADGHIMAGE,
	RELVERIFY+GADGIMMEDIATE+TOGGLESELECT,
	BOOLGADGET,
	NULL,
	NULL,
	&midiinIText7,
	0,
	NULL,
	1,
	NULL
};

#define midiinGadgetList1 midiinGadget1

struct NewWindow midiinNewWindowStructure1 = {
	134,50,
	156,95,
	6,1,
	GADGETUP+CLOSEWINDOW,
	WINDOWDRAG+WINDOWDEPTH+WINDOWCLOSE+ACTIVATE+NOCAREREFRESH,
	&midiinGadget1,
	NULL,
	"MIDI In",
	NULL,
	NULL,
	5,5,
	-1,-1,
	CUSTOMSCREEN
};

