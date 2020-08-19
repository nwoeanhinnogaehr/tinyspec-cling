// the (dis)connect function takes two JACK client names, optional port filters, and an optional mode parameter.
//
// void    connect(string client_a, string client_b, string filter_a="", string filter_b="", int mode=CONNECT_MAX);
// void disconnect(string client_a, string client_b, string filter_a="", string filter_b="", int mode=CONNECT_MAX);
//
// you can use the global CLIENT_NAME to get the name of the current tinyspec instance.

// as a basic example, the following will connect the system speakers and mic to tinyspec.
connect(CLIENT_NAME, "system");

// to undo the operation, we can call disconnect with the same arguments.
disconnect(CLIENT_NAME, "system");

// filters let us choose which ports to connect.
// a filter of "o" selects only output ports, and a filter of "i" selects only input ports.
// for example, the following will connect tinyspec output to the system speakers. 
connect(CLIENT_NAME, "system", "o", "i");

// since we can only pair up outputs and inputs, the filter is often implied for one of the clients.
// the previous example is equivalent to the following.
connect(CLIENT_NAME, "system", "o");

// if we only wish to connect one of the channels, we can put a number in the filter to select it.
// the number is indexed starting from 0. 
// the following connects tinyspec channel 0 and system channel 1.
connect(CLIENT_NAME, "system", "0", "1"); 

// this can be combined with the input and output filters but putting the number after the filter.
connect(CLIENT_NAME, "system", "i0", "1"); 

// when there is a mismatch in the number of channels, tinyspec will duplicate or mix channels as
// necessary to connect all specified ports. it determines which ports to match up by wrapping around
// to the beginning of the list of selected channels for the side of the connection with the smaller
// number of selected channels.
// for example the following connects output 0 of tinyspec to all system speakers.
connect(CLIENT_NAME, "system", "o0");

// we can also specify a range a ports to connect.
// for example the range "o1,3" selects output ports 1 and 2 (the bounds are inclusive,exclusive)
set_num_channels(4, 4);
connect(CLIENT_NAME, "system", "o1,3");

// we can now demonstrate more clearly what happens when there is a mismatch in the number of channels.
// we select 3 channels from tinyspec and 2 from the system output. the result is that tinyspec channel
// 1 and 3 are both connected to system channel 0 and tinyspec channel 2 is connected to system channel 1.
// in total 3 connections are made = max(2,3). this is the meaning of CONNECT_MAX (the default mode).
connect(CLIENT_NAME, "system", "o1,4", "0,2");

// to prevent connecting multiple wires to the same port, pass CONNECT_MIN as the mode.
// the following only connects tinyspec channel 1 to system channel 0 and tinyspec channel 2
// to system channel 1, ignoring tinyspec channel 3.
connect(CLIENT_NAME, "system", "o1,4", "0,2", CONNECT_MIN);

// to connect all matching outputs to all matching outputs we can use CONNECT_ALL.
// the following disconnects tinyspec completely from everything else.
disconnect(CLIENT_NAME, ".*", "", "", CONNECT_ALL);

// we can also leave off either of the bounds. if we do so the lower bound is implicitly 0
// and the upper bound is implicitly infinity.
// the following connects all output ports to the system but the indexing is off by 1.
connect(CLIENT_NAME, "system", "o1,");

// client names can be filtered using regular expressions.
// the syntax is documented at https://laurikari.net/tre/documentation/regex-syntax/
// for example the following connects tinyspec to every other client (including itself, unless self
// connection requests are ignored by your JACK server).
connect(CLIENT_NAME, ".*");

// ports can also be selected by name, or regular expressions, by starting the filter with a colon.
// since the tinyspec output ports match the regex "out.*" we can alternatively select them with:
connect(CLIENT_NAME, "system", ":out.*");

// the following routes the audio output of all PulseAudio application through tinyspec.
disconnect("PulseAudio JACK Sink", "system");
connect(CLIENT_NAME, "system", "o");
connect(CLIENT_NAME, "PulseAudio JACK Sink");

// reconnect pulse to the speakers:
connect("PulseAudio JACK Sink", "system");
disconnect(CLIENT_NAME, "system", "o");
disconnect(CLIENT_NAME, "PulseAudio JACK Sink");

// feed back renoise through tinyspec
connect(CLIENT_NAME, "renoise", "", "i2,", CONNECT_MIN);
connect(CLIENT_NAME, "renoise", "", "o2,", CONNECT_MIN);
