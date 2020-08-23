/* Example of controlling tinyspec with tidalcycles via OSC
   Adapted from simple-pitchshift.cpp

tinyspecTarget = OSCTarget {oName = "Tinyspec", oAddress = "127.0.0.1", oPort = 44101, oPath = "/shift", oShape = Just [("shift", Just $ VF 0)], oLatency = 0.02, oPreamble = [], oTimestamp = MessageStamp}
tidal <- startTidal tinyspecTarget defaultConfig
let p = streamReplace tidal
let shift = pF "shift"
p 1 $ sometimes rev $ shift "0.5 0.75 1 1.25 1.5 1.75 2"

*/

connect(CLIENT_NAME, "system");
set_process_fn([&](WaveBuf &in, WaveBuf &out, double t){
    static float shift = 1;
    for (auto msg : osc_recv(44101, t, "/shift")) {
        osc_get(*msg, shift);
        if (shift < 0.1) shift = 0.1;
    }
    window_hann(in);
    out.resize(in.num_channels, in.size/shift);
    for (size_t i = 0; i < in.num_channels; i++)
        for (size_t j = 0; j < in.size; j++)
            for (int k = j/shift; k < (j+1)/shift; k++)
                out[i][k] = in[i][j];
    next_hop_ratio(8192,0.5/shift);
});
