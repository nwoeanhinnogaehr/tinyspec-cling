// connect tinyspec to system speakers
system("jack_connect " CLIENT_NAME ":out0 system:playback_1");
system("jack_connect " CLIENT_NAME ":out1 system:playback_2");

// feed back renoise through tinyspec
system("jack_connect " CLIENT_NAME ":out0 renoise:input_01_left");
system("jack_connect " CLIENT_NAME ":out1 renoise:input_01_right");
system("jack_connect renoise:output_02_left " CLIENT_NAME ":in0");
system("jack_connect renoise:output_02_right " CLIENT_NAME ":in1");

// detour pulse through tinyspec
system("jack_disconnect \"PulseAudio JACK Sink:front-left\" system:playback_1");
system("jack_disconnect \"PulseAudio JACK Sink:front-right\" system:playback_2");
system("jack_connect " CLIENT_NAME ":out0 system:playback_1");
system("jack_connect " CLIENT_NAME ":out1 system:playback_2");
system("jack_connect \"PulseAudio JACK Sink:front-left\" " CLIENT_NAME ":in0");
system("jack_connect \"PulseAudio JACK Sink:front-right\" " CLIENT_NAME ":in1");

// tinyspec to renoise
system("jack_connect " CLIENT_NAME ":out0 renoise:input_01_left");
system("jack_connect " CLIENT_NAME ":out1 renoise:input_01_right");

// Revert pulse to normal
system("jack_connect \"PulseAudio JACK Sink:front-left\" system:playback_1");
system("jack_connect \"PulseAudio JACK Sink:front-right\" system:playback_2");

// system in to renoise
system("jack_connect system:capture_1 renoise:input_01_left");
system("jack_connect system:capture_2 renoise:input_01_right");

// system in through tinyspec to renoise
system("jack_disconnect system:capture_1 renoise:input_01_left");
system("jack_disconnect system:capture_2 renoise:input_01_right");
system("jack_connect system:capture_1 " CLIENT_NAME ":in0");
system("jack_connect system:capture_2 " CLIENT_NAME ":in1");
system("jack_connect " CLIENT_NAME ":out0 renoise:input_01_left");
system("jack_connect " CLIENT_NAME ":out1 renoise:input_01_right");
