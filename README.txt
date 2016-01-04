S4 project

Martin VASSOR - Sciper 223825

prerequisite programs : 
 - make
 - gcc 

prerequisite library : 
 - traditionnal C stuff (pthread, string, socket, etc...), look at the headers of .[ch] files for exhaustive list.

prerequisite rights : 
 - the lunching script assume to have the read/write/execute rights in the current directory

Caution for grading : 
 - The script creates a launch script (which basically run the binary), and then execute the launch script, which himself runs the binary. It can do some troubles when the evaluation scripts send signals (dunno which program intercept them). Also, the binary internally run pthreads, which can do a bit the same problem (I don't know if signals are propagated to LWT).

Execution procedure : 
make  # produces s4.bin
./s4 args... # run the script which manage the trap and the redirection

The output file has the same name than the input one, with the extension replaced by .output


For more general use (more nodes, etc...) you can directly run s4.bin : 
./s4.bin addr1 port1 [addrn portn] index inputfile

