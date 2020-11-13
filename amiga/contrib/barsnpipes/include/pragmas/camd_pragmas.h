#ifndef PRAGMAS_CAMD_PRAGMAS_H
#define PRAGMAS_CAMD_PRAGMAS_H

#ifndef CLIB_CAMD_PROTOS_H
#include <clib/camd_protos.h>
#endif

#pragma  libcall CamdBase LockCAMD               01e 001
#pragma  libcall CamdBase UnlockCAMD             024 801
#pragma  libcall CamdBase CreateMidiA            02a 801
#pragma  libcall CamdBase DeleteMidi             030 801
#pragma  libcall CamdBase SetMidiAttrsA          036 9802
#pragma  libcall CamdBase GetMidiAttrsA          03c 9802
#pragma  libcall CamdBase NextMidi               042 801
#pragma  libcall CamdBase FindMidi               048 901
#pragma  libcall CamdBase FlushMidi              04e 801
#pragma  libcall CamdBase AddMidiLinkA           054 90803
#pragma  libcall CamdBase RemoveMidiLink         05a 801
#pragma  libcall CamdBase SetMidiLinkAttrsA      060 9802
#pragma  libcall CamdBase GetMidiLinkAttrsA      066 9802
#pragma  libcall CamdBase NextClusterLink        06c 09803
#pragma  libcall CamdBase NextMidiLink           072 09803
#pragma  libcall CamdBase MidiLinkConnected      078 801
#pragma  libcall CamdBase NextCluster            07e 801
#pragma  libcall CamdBase FindCluster            084 801
#pragma  libcall CamdBase PutMidi                08a 0802
#pragma  libcall CamdBase GetMidi                090 9802
#pragma  libcall CamdBase WaitMidi               096 9802
#pragma  libcall CamdBase PutSysEx               09c 9802
#pragma  libcall CamdBase GetSysEx               0a2 09803
#pragma  libcall CamdBase QuerySysEx             0a8 801
#pragma  libcall CamdBase SkipSysEx              0ae 801
#pragma  libcall CamdBase GetMidiErr             0b4 801
#pragma  libcall CamdBase MidiMsgType            0ba 801
#pragma  libcall CamdBase MidiMsgLen             0c0 001
#pragma  libcall CamdBase ParseMidi              0c6 09803
#pragma  libcall CamdBase OpenMidiDevice         0cc 801
#pragma  libcall CamdBase CloseMidiDevice        0d2 801
#pragma  libcall CamdBase RethinkCAMD            0d8 00
#pragma  libcall CamdBase StartClusterNotify     0de 801
#pragma  libcall CamdBase EndClusterNotify       0e4 801
#ifdef __SASC_60
#pragma  tagcall CamdBase CreateMidi             02a 801
#pragma  tagcall CamdBase SetMidiAttrs           036 9802
#pragma  tagcall CamdBase GetMidiAttrs           03c 9802
#pragma  tagcall CamdBase AddMidiLink            054 90803
#pragma  tagcall CamdBase SetMidiLinkAttrs       060 9802
#pragma  tagcall CamdBase GetMidiLinkAttrs       066 9802
#endif

#endif	/*  PRAGMAS_CAMD_PRAGMA_H  */
