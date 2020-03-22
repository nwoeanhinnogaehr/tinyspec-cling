// connect tinyspec to system speakers
system("jack_connect tinyspec_cmd1:out0 system:playback_1");
system("jack_connect tinyspec_cmd1:out1 system:playback_2");

// feed back renoise through tinyspec
system("jack_connect tinyspec_cmd1:out0 renoise:input_01_left");
system("jack_connect tinyspec_cmd1:out1 renoise:input_01_right");
system("jack_connect renoise:output_02_left tinyspec_cmd1:in0");
system("jack_connect renoise:output_02_right tinyspec_cmd1:in1");

// detour pulse through tinyspec
system("jack_disconnect \"PulseAudio JACK Sink:front-left\" system:playback_1");
system("jack_disconnect \"PulseAudio JACK Sink:front-right\" system:playback_2");
system("jack_connect tinyspec_cmd1:out0 system:playback_1");
system("jack_connect tinyspec_cmd1:out1 system:playback_2");
system("jack_connect \"PulseAudio JACK Sink:front-left\" tinyspec_cmd1:in0");
system("jack_connect \"PulseAudio JACK Sink:front-right\" tinyspec_cmd1:in1");

// tinyspec to renoise
system("jack_connect tinyspec_cmd1:out0 renoise:input_01_left");
system("jack_connect tinyspec_cmd1:out1 renoise:input_01_right");

// Revert pulse to normal
system("jack_connect \"PulseAudio JACK Sink:front-left\" system:playback_1");
system("jack_connect \"PulseAudio JACK Sink:front-right\" system:playback_2");
