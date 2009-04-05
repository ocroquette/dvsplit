dvsplit is a simple tool to split a big raw DV file in smaller DV files based on the timestamps of the DV frames.
Included are the source code and a MacOS X binary.

The usage is quite simple :
	dvsplit INPUTFILE.dv OUTPUTPREFIX

The tool will read INPUTFILE, and at each discontinuity of the timestamp generate a new file named OUTPUTPREFIX_nnn.dv

Note that it will probably work only with PAL material.

It's based on libdv : http://libdv.sourceforge.net/
