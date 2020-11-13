# CAMD Port I/O Tools for Bars'n'Pipes Pro

This directory contains the source for creating BnP .ptool files for
the new CAMD drivers of this project.

Its based on the CAMD Tool for the MMP driver written by Kjetil.S.Matheussen: See [CAMDlibrary40_4&BnPTools.lha](http://bnp.hansfaust.de/download/CAMDlibrary40_4&BnPTools.lha) for the original source.

The sources need to be compiled with SAS/C 6.58.

Simply run:

    smake CAMD_NAME=udp CAMD_PORT=0

to create in and out tools for CAMD driver *udp* and port 0.
The resulting tools are named:

* *camd_udp_in_0.ptool* for the input device
* *camd_udp_out_0.ptool* for the output device
