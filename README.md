# amiditools

My collection of tools for MIDI processing focusing on classic m68k Amiga
machine running AmigaOS 3.x with the CAMD MIDI library. Enriched by some host
tools running on Python >3.6.

## Overview

### CAMD MIDI Drivers

* [`udp`](#udp-driver) - A network MIDI driver with custom protocol
* [`echo`](#echo-driver) - A simple (test) driver that echoes all MIDI data

### CAMD MIDI Tools

* [`midi-info`](#midi-info) - Show CAMD drivers and more
* [`midi-send`](#midi-send) - Send MIDI data via command line
* [`midi-recv`](#midi-recv) - Receive MIDI data on command line
* [`midi-echo`](#midi-echo) - Echo incoming MIDI traffic
* [`midi-perf`](#midi-perf) - MIDI performance measurement

### CAMD Addons

* [BarsnPipes Tools](#barnspipes-tools) - BarsnPipes Tools to access the new
  CAMD MIDI drivers

### Host MIDI Tools

* [`midi-udp-bridge`](#midi-udp-bridge) - Endpoint for `udp` MIDI driver
* [`midi-udp-echo`](#midi-udp-echo) - Test endpoint for `udp` MIDI driver
* [`midi-perf`](#midi-perf-host) - MIDI performance measurement


## Installation

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

### Install CAMD Drivers

The CAMD drivers shipped with the tools follow the naming scheme `midi-drv-*`.
In order to install them on your Amiga system you have to strip the
`midi-drv-` prefix and copy the files to the `devs:midi` directory.

Example:

    copy midi-drv-udp devs:midi/udp

A restart of the Amiga is required to activate the new drivers.
Use the [`midi-info`](#midi-info) tool to check its availability.

### Host Tools Installation

All the tools here running on your Mac or PC are written in `Python 3`
(Python >=3.6 required).

For MIDI handling they use the
[`python-rtmidi`](https://pypi.org/project/python-rtmidi/)
package. Simply install it with:

    pip3 install python-rtmidi

Then you can run the host tool scripts directly in place of the release
directory.

However, also a `.whl` Python install archive is shipped. You can install
it with:

    pip3 install amiditools-*.whl


## CAMD MIDI Drivers

### `echo` Driver

The echo driver is a very simple driver without any options.
It offers 8 input and output endpoints named:

 * Inputs `echo.in.0` ... `echo.in.7`
 * Output `echo.out.0` ... `echo.out.7`

All MIDI data sent to an output is echoed to the corresponding input.

    midi-send --> echo.out.0 --> echo.in.0 --> midi-recv

#### Usage Example

 * Open an Amiga shell and launch [`midi-recv`](#midi-recv):

        midi-recv dev echo.in.0

 * Now open another shell and send some MIDI messages with
   [`midi-send`](#midi-send):

        midi-send dev echo.out.0 on c2 127
        midi-send dev echo.out.0 off c2 0

 * You see that the MIDI messages are forwarded from output 0 to input 0

### `udp` Driver

This driver allows to send MIDI data across an network link provided by
an Amiga network stack (like AmiTCP or Roadshow). It uses simple UDP packets
to transfer MIDI messages (including SysEx) and offers up to 8 individual
in and out ports:

 * Inputs `udp.in.0` ... `udp.in.7`
 * Output `udp.out.0` ... `udp.out.7`

A client application called [`midi-udp-bridge`](#midi-udp-bridge) on your PC
or Mac will receive the MIDI message and forward them to a local MIDI port
there.

All MIDI data sent to the output `udp.out.x` will be sent to the bridge
host output port and the input `udp.in.x` will receive all data sent to
the input port of the host bridge (option `-p IN:OUT`):

    midi-send --> udp.out.x --> network --> midi-udp-bridge --> OUT midi port
    midi-recv <-- udp.in.x <-- network <-- midi-udp-bridge <-- IN midi port

Note: Do not confuse this protocol with the well known [RTP
MIDI](https://john-lazzaro.github.io/rtpmidi/) or also called Apple MIDI. Our
protocol is a lot simpler but needs a special host program to send/receive
data to/from this driver.

Note2: This protocol currently does no error correction. I.e. if your network
is crowded or lots of MIDI traffic is transferred then some MIDI messages
might get lost. You have been warned :)

#### Activation

First of all make sure that your Amiga network stack is already setup and
all network devices are configured correctly.

Use a tool like [`midi-info`](#midi-info) to load the CAMD library and
therefore activate the UDP driver.

When the driver is loaded on first launch of the `camd.library` then it
opens a UDP server socket and waits for an incoming client.

The client is created then on the host Mac or PC side with the
[`midi-udp-bridge`](#midi-udp-bridge) tool.

#### Configuration

By default the driver is configured to work with settings that match most
typical use cases, including emulator and real Amiga setups.

However, sometimes adjustments are needed. You write a config file with the
required parameters. The driver looks for the configuration file in
`ENV:midi/udp.config`.

Create this file and write all configuration options (that are `ReadArgs()`-
like) into the first line of the file!

The following options are supported:

* `HOST_NAME <hostname|ipaddr>`

   Set the host name or IPv4 address that will be used to open the UDP server
   socket.

   By default its `0.0.0.0` and binds to all network interfaces.

* `PORT <number>`

    Change the port number the UDP server will use on the Amiga.

    By default its `6820`.

* `SYSEX_SIZE <bytes>`

    Currently the `udp` driver can only send SysEx messages with up to
    2048 Bytes (2 KiB). If you need to transfer larger SysEx messages
    then increase this byte value.

    Please note that currently all SysEx messages have to fit into a single
    UDP packet. So the limit lies around 64 KiB per single message.

An Example config might look like:

    HOST_NAME my_amiga PORT 1234 SYSEX_SIZE 32768

Copy the config file to `ENVARC:midi/udp.config` to persist it for later use.

### Usage Example

 * Make sure the `udp` driver is running and call [`midi-info`](#midi-info)
 * Now on your Mac/PC call [`midi-udp-bridge`](#midi-udp-bridge) and
   assign a distinct MIDI in and out port (I use virtual ports on Mac here):

        midi-udp-bridge -p "Amiga In:Amiga Out" -v

 * If all worked out well then the connection is established and reported back
   by the bridge
 * On your Mac/PC run [`receivemidi`](https://github.com/gbevin/ReceiveMIDI)
   to see the incoming traffic on port "Amiga Out"

        receivemidi dev "Amiga Out"

 * Now send some MIDI messages from your Amiga shell:

        midi-send dev udp.out.0 on c2 127
        midi-send dev udp.out.0 off c2 0

 * You should see the messages on your Mac/PC on port "Amiga Out".
 * One direction already works! Now test the other one:
 * On your Amiga shell run:

        midi-recv dev udp.in.0

 * On your Mac/PC send MIDI data with
   [`sendmidi`](https://github.com/gbevin/SendMIDI):

        sendmidi dev "Amiga In" on c2 127
        sendmidi dev "Amiga In" off c2 0

 * You should see the messages on your Amiga shell
 * Both directions work! Job done :)


## CAMD MIDI Tools

All tools are command line tools and need to be executed in a shell/CLI.

### `midi-info`

This tool simply shows all available drivers with their registered clusters
(aka ports).

Use the tool to quickly verify that new drivers are installed correctly.

A nice side effect of this tool is: When launched as the first command after
a reboot it initializes the `camd.library` and activates all drivers.
E.g. the `udp` driver starts listening for a UDP client.

Note: As long as the library is not expunged, it will not reload the driver
list. Therefore you might need an Amiga reset to activate new drivers.

### `midi-send`

This tool is an almost 100% clone of the famous [`SendMIDI`
tool](https://github.com/gbevin/SendMIDI) by Geert Bevin.

You can send all kinds of MIDI message on the command line.

See the [`SendMIDI` README](https://github.com/gbevin/SendMIDI#readme) for
a detailed description of the options.

Example: Send a NoteOn/NoteOff for C-2 with velocity 127

    midi-send dev echo.in.0 on c-2 127
    midi-send dev echo.in.0 off c-2 0

#### Notable Differences to SendMIDI

* `dev` - uses CAMD cluster names, e.g. `echo.in.0`
 * no `virt` - no virtual device on Amiga
 * no `list` - use [`midi-info`](#midi-info) instead
 * no `mpe*` - not implemented yet
 * no long names of commands
 * added `SMS=SYSEXMAXSIZE` to select maximum size of SysEx messages in bytes
   (default: 2048)
 * added `V=VERBOSE` to show more output

### `midi-recv`

This tool is an almost 100% clone of the famous [`ReceiveMIDI`
tool](https://github.com/gbevin/ReceiveMIDI) by Geert Bevin.

It will show all incoming MIDI messages in your shell.
Until you leave the tool by pressing `CTRL+C`.

See the [`ReceiveMIDI` README](https://github.com/gbevin/ReceiveMIDI#readme)
for a detailed description of the options.

Example: show incoming traffic on `echo.out.0`

    midi-recv dev echo.out.0

### Notable Differences to ReceiveMIDI

* `dev` - uses CAMD cluster names, e.g. `echo.in.0`
 * no `virt` - no virtual device on Amiga
 * no `pass` - no implemented yet
 * no `list` - use [`midi-info`](#midi-info) instead
 * no `js*`- no javascript support
 * added `SMS=SYSEXMAXSIZE` to select maximum size of SysEx messages in bytes
   (default: 2048)
 * added `V=VERBOSE` to show more output

### `midi-echo`

A tool that echoes all MIDI data received on one port and writes it to
another one.

Stop the tool by pressing `CTRL+C`.

Usage:

    midi-echo  IN/A input OUT/A output
               SMS/SYSEXMAXSIZE/K/N sysex_max_size
               V/VERBOSE/S

Options:

 * `SMS=SYSEXMAXSIZE` sets the maximum size of SysEx messages in bytes.
   Default value is 2048 bytes.
 * `V=VERBOSE` be more verbose when running the tool

Example:

    midi-echo IN echo.out.0 OUT echo.in.1

### `midi-perf`

A tool to measure the performance of a MIDI driver.

It sends out bursts of MIDI data to the given ouput device port and tries to
receives the same messages on the given input devices. The latency is
calculated and missing messages are detected.

You have to create a loop back setup to make this tool work. You can either
use the [`echo`](#echo-driver) driver directly (as it loops directly) or use
the [`udp`](#udp-driver) driver in combination with a
[`midi-udp-echo](#midi-udp-echo) host client to loop back MIDI messages over
UDP.

The tool continously sends out a given number of samples in a loop and waits
for the incoming echoed samples.

Currently, each sample pair is a NoteOn and NoteOff event with increasing
note value.

Usage:

    midi-perf  OUTDEV/A  output  INDEV/A  input
               SMS/SYSEXMAXSIZE/K/N sysex_max_size
               V/VERBOSE/S
               LP=LOOPDELAY/K/N loop_delay
               SD=SAMPLEDELAY/K/N sample_delay
               NUM/K/N number_of_samples

Options:

 * `SMS=SYSEXMAXSIZE` sets the maximum size of SysEx messages in bytes.
   Default value is 2048 bytes.
 * `V=VERBOSE` be more verbose when running the tool
 * `LP=LOOPDELAY` how many seconds to wait before another sample loop is sent.
   Default is 1 second.
 * `SD=SAMPLEDELAY` how many microseconds to wait between each test sample.
   Default is 1000 microseconds.
 * `NUM` number of sample MIDI messages sent in a loop. Default is 256.

Example:

    midi-perf udp.out.0 udp.in.0
    midi-perf echo.out.0 echo.in.0

## CAMD Addons

### BarnsPipes Tools

To access the new CAMD drivers presented here with the [BarsnPipes Sequencer]
(http://bnp.hansfaust.de) on the Amiga a "ptool" plugin is available for each
of the drivers and input/output clusters/ports combinations.

Just add the ptools to your BarnsPipes installation and activate them.
See the BnP manual for details.

The tools are named `camd_<driver>_in|out_<port>.ptool`.

## Host Tools

### `midi-udp-bridge`

This tool is the endpoint of the [`udp`](#udp-driver) MIDI driver. Launch it
on your Mac/PC. It will bridge the incoming MIDI traffic to existing (or
virtual) MIDI ports on your system.

Currently, a single client is supported that maps all the input/output
ports of the `udp` driver to local MIDI ports.

#### Options

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

Use `-s` and `-c` switches to configure the hostname of the server (Amiga) and
the client (your Mac/PC) respectively. The syntax for both is `hostname:port`.

Examples for address specification:

 * `192.168.2.1:6820`
 * `my_amiga:1234`

Use `-l` to show a list of existing MIDI in and out ports on your system.

Use `-v` or even `-d` to increase the verbosity when running the tool.

#### MIDI Port Mapping

The only required option is `-p` for the ports definition. Here you assign a
MIDI in and out port pair of your system to the ports of the Amiga UDP driver
separated by a colon (`:`).

The first port definition pair is assigned to udp port 0, the next one to port
1 and so on. Up to 8 ports can be defined this way.

You can either state the name of the interface (see `-l` command for a list)
or its enumerated number.

If you do not need a in and out port you can simply omit one direction.
If only an output port is given then do not forget the leading colon!

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
    your system with the given names (if your system supports this).

#### Run the Tool

So a minimal example to run the tool is:

    python3 midi-udp-bridge -p 0:0 -d

Use the debug output `-d` to see the inner working of the bridge.

### `midi-udp-echo`

This is a diagnosis tool: it simply returns all incoming MIDI messages back
to port 0 (`udp.in.0`).

Launch it with:

    python3 midi-udp-echo

It will wait for incoming MIDI messages and return them.

### `midi-perf` Host

A MIDI performance measurment tool running on your host.

A MIDI port pair is required that performs loop back of all messages.

#### Options

    usage: midi-perf [-h] [-p PORT] [-l] [-v] [-d]

    benchmark Midi performance by sending/receiving a set of messages.

    optional arguments:
    -h, --help            show this help message and exit
    -p PORT, --port PORT  Define midi port pair: midi_in:midi_out[+] Make sure out echoes in data!
    -l, --list-ports      List all input and output ports
    -v, --verbose         verbose output
    -d, --debug           enabled debug output
