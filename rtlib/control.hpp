double current_time_secs();
using func_t = function<void(WaveBuf&, WaveBuf&)>;
namespace internals {
    extern uint64_t max_backbuffer_size;
    extern uint64_t latency;
    extern uint64_t block_time, block_size;
    extern deque<float> aoutqueue, ainqueue, backbuffer;
    extern double time_now;
    extern size_t nch_in, nch_out;
    extern mutex data_mtx;
    extern double rate;
    extern vector<jack_port_t *> in_ports;
    extern vector<jack_port_t *> out_ports;
    extern jack_client_t *client;
}

double sample_rate() {
    return internals::rate;
}
void set_num_channels(size_t in, size_t out) {
    lock_guard<mutex> data_lk(internals::data_mtx);

    if (in != internals::nch_in) {
        internals::ainqueue.clear();
        internals::backbuffer.clear();
        for (size_t i = in; i < internals::nch_in; i++) {
            jack_port_unregister(internals::client, internals::in_ports.back());
            internals::in_ports.pop_back();
        }
        for (size_t i = internals::nch_in; i < in; i++) {
            string name = "in" + to_string(i);
            internals::in_ports.push_back(jack_port_register(internals::client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0));
        }
    }

    if (out != internals::nch_out) {
        internals::aoutqueue.clear();
        for (size_t i = out; i < internals::nch_out; i++) {
            jack_port_unregister(internals::client, internals::out_ports.back());
            internals::out_ports.pop_back();
        }
        for (size_t i = internals::nch_out; i < out; i++) {
            string name = "out" + to_string(i);
            internals::out_ports.push_back(jack_port_register(internals::client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));
        }
    }
    internals::nch_in = in;
    internals::nch_out = out;
}
double now_secs() {
    return now()/internals::rate;
}
double now() {
    return internals::time_now;
}
void set_time_secs(double secs) {
    /*if (secs < 0)
        cerr << "negative time is not supported, ignoring call to set_time_secs(" << secs << ")" << endl;
    else {
        base_time += time_secs() - secs;
        internals::time_samples = uint64_t(secs*internals::rate);
        internals::time_fract = fmod(secs*internals::rate, 1.0)*double(uint64_t(-1));
    }*/
    assert(0); // TODO
}
void set_now(double samples) {
    //set_time_secs(samples/internals::rate);
    assert(0); // TODO
}
WaveBuf get_input_back(double t, size_t n) {
    return get_input_at(now()-t-n, n);
}
WaveBuf get_input_at(double t, size_t n) {
    using namespace internals;
    WaveBuf out(nch_in, n);
    for (size_t c = 0; c < nch_in; c++) {
        for (size_t i = 0; i < n; i++) {
            size_t idx = (i + t - block_time - block_size + backbuffer.size()/nch_in)*nch_in + c;
            if (idx < backbuffer.size())
                out[c][i] = backbuffer[idx];
        }
    }
    return out;
}
void set_max_backbuffer_size(size_t n) {
    internals::max_backbuffer_size = n;
}
size_t max_backbuffer_size() {
    return internals::max_backbuffer_size;
}
void set_latency(size_t n) {
    lock_guard<mutex> data_lk(internals::data_mtx);
    for (size_t i = internals::latency; i < n; i++)
        for (size_t c = 0; c < internals::nch_out; c++) {
            internals::aoutqueue.push_back(0);
        }
    for (size_t i = n; i < internals::latency; i++)
        for (size_t c = 0; c < internals::nch_out; c++)
            internals::aoutqueue.pop_front();
    internals::latency = n;
}
size_t latency() {
    return internals::latency;
}
