# CAMD Port I/O Tools for Bars'n'Pipes Pro

This directory contains the source for creating BnP .ptool files for
the new CAMD drivers of this project.

Its based on the CAMD Tool for the MMP driver written by Kjetil.S.Matheussen: See [CAMDlibrary40_4&BnPTools.lha](http://bnp.hansfaust.de/download/CAMDlibrary40_4&BnPTools.lha) for the original source.

The sources here were ported to vbcc.

Simply run:

    make all

to create in and out tools for CAMD driver *udp* and *echo* for various ports.
The resulting tools are named:

* *camd_udp_in_0.ptool* for the input device on port 0
* *camd_udp_out_0.ptool* for the output device on port 0
