# miditools

My collection of tools for MIDI processing focusing on classic Amiga machine
running AmigaOS 3.x. Enriched by some host tools running on Python 3.

## Prerequisites

### CAMD Library

You need a recent version of the CAMD libraries installed on your Amiga system
in order to use the drivers or tools on this page.

I'd suggest to use CAMD.library 40.4 or 40.5 available on
[aminet](http://aminet.net/mus/midi/camd40.lha). Just copy the `camd.library`
to your `LIBS:` directory.

Midi drivers for CAMD are found in the `devs:midi` directory. Just create
the folder manually if its missing.

To use a MIDI interface attached to the serial port of your Amiga you have
to install the [mmp](http://aminet.net/driver/other/mmp.lha) driver.
Just copy the driver file `mmp` found in the archive to your `devs:midi`
directory and you are done!

### Host Tools: Python 3 and `python-rtmidi` package

All the tools here running on your Mac or PC are written in `Python 3`.

For MIDI handling they use the
[`python-rtmidi`](https://pypi.org/project/python-rtmidi/) 
package. Simply install it with:

    pip3 install python-rtmidi

## `udp` Network MIDI for your Amiga

This driver allows to send MIDI data across an network link provided by
an Amiga network stack (like AmiTCP or Roadshow). It uses simple UDP packets
to transfer MIDI messages (including SysEx) and offers up to 8 individual
in and out ports.

Do not confuse this protocol with the well known 
[RTP MIDI](https://john-lazzaro.github.io/rtpmidi/) or also called
Apple MIDI. This protocol is a lot simpler but needs a special host program
called `midi-udp-bridge` on your PC or Mac to send/receive data to/from this
driver.

### Installation

Simply copy the supplied driver `midi-drv-udp` to `devs:midi/udp`. Make sure
to call the driver `udp` only!

Use a tool like `midi-info` (see below) to verify that the driver was
found by CAMD.

### Configuration

The driver looks for a configuration file stored in `ENV:midi/udp.config`.

Create this file and write all configuration options (that are ReadArgs())
like into the first line of the file!

The following options are supported:

* `CLIENT_HOST <hostname|ip_addr>`

    Set the host name of the Amiga client. You can use an IP address or
    a host name that your Amiga TCP stack can resolve.

    By default its `localhost` for use in an emulator (see below).

* `CLIENT_PORT <number>`

    Change the port number the client (Amiga) receives MIDI messages on.

    By default its `6821`.

* `SERVER_HOST <hostname|ipaddr>`

   Set the host name of the machine that will receive the MIDI messages
   from this driver.

   By default its `localhost` for use in an emulator (see below).

* `SERVER_PORT <number>`

    Change the port number the server (PC) receives MIDI messages on.

    By default its `6820`.

* `SYSEX_SIZE <bytes>`

    Currently the `udp` driver can only send SysEx messages with up to
    2048 Bytes (2 KiB). If you need to transfer larger SysEx messages
    then increase this value.

    Please note that currently all SysEx messages have to fit into a single
    UDP packet. So the limit lies around 64 KiB per single message.

An Example config might look like:

    SERVER_HOST my_mac SERVER_PORT 1234 CLIENT_HOST my_amiga CLIENT_PORT 4321

Copy the config file to `ENVARC:midi/udp.config` to persist it for next use.

### Host Tool: `midi-udp-bridge`

This tool is the endpoint of the `udp` MIDI driver. Launch it on your Mac/PC.
It will bridge the incoming MIDI traffic to existing (or virtual) MIDI ports
on your system.

#### Options

```
usage: midi-udp-bridge [-h] [-p PORTS [PORTS ...]] [-l] [-v] [-d] [-s SERVER] [-c CLIENT]

transfer Midi data between Midi UDP and a local Midi port

optional arguments:
  -h, --help            show this help message and exit
  -p PORTS [PORTS ...], --ports PORTS [PORTS ...]
                        Define midi port pair: midi_in:midi_out[+] Repeat for multiple ports.
  -l, --list-ports      List all input and output ports
  -v, --verbose         verbose output
  -d, --debug           enabled debug output
  -s SERVER, --server SERVER
                        host addr of UDP server. default=localhost:6820
  -c CLIENT, --client CLIENT
                        host addr of UDP client. default=localhost:6821
```

Use `-s` and `-c` switches to configure the hostname of the server and the
client respectively. The syntax for both is `hostname:port`.

Examples for address specification:

 * `192.168.2.1:6820`
 * `my_amiga:1234`

Use `-l` to show a list of existing MIDI in and out ports on your system.

Use `-v` or even `-d` to increase the verbosity when running the tool.

The only required option is `-p` for the ports definition. Here you assign
a MIDI in and out port of your system to the ports of the Amiga UDP driver.

The first port definition is assigned to udp port 0, the next one to port 1
and so on. Up to 8 ports can be defined this way.

You can either state the name of the interface (see `-l` command for a list)
or its enumerated number.

If you do not need a in and out port you can simply omit one direction.

If you want to create a new *virtual* MIDI port (only supported on Linux
or macos) then append a plus `+` sign to your definition. Both the in and
out port are then virtual ports.

Examples for port specification:

 * `-p "Amiga In:Amiga Out"` - define udp port 0 and connect to the existing
    in port called `Amiga In` and out port `Amiga Out`. Use shell quoting to
    protect the spaces in the interface names.
 * `-p 0:0 1:1` - define udp port 0 and 1 by referencing the interfaces by
    number. Use first in and out interface for port 0 and second pair for
    port 1.
 * `-p :0` - only define the udp output port and omit the input port.
 * `-p MyIn:MyOut+` - the trailing plus creates new virtual interfaces in
    your system with the given names.

#### Run the Tool

So a minimal example to run the tool is:

    python3 midi-udp-bridge -p 0:0 -d

Use the debug output `-d` to see the inner working of the bridge.

### Host Tool: `midi-udp-echo`

This is a diagnosis tool: it simply returns all incoming MIDI messages back
to port 0 (`udp.in.0`).

Launch it with:

    python3 midi-udp-echo

It will wait for incoming MIDI messages and return them.
