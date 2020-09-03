double current_time_secs();
using func_t = function<void(WaveBuf&, WaveBuf&, double)>;
namespace internals {
    extern uint64_t window_size;
    extern uint64_t new_window_size;
    extern uint64_t hop_samples;
    extern uint64_t hop_fract;
    extern uint64_t computed_hop;
    extern double hop;
    extern deque<float> aqueue, atmp, ainqueue;
    extern uint64_t time_samples;
    extern uint64_t time_fract;
    extern size_t nch_in, nch_out, new_nch_in, new_nch_out;
    extern mutex data_mtx;
    extern func_t fptr;
    extern double rate;
    extern vector<jack_port_t *> in_ports;
    extern vector<jack_port_t *> out_ports;
    extern jack_client_t *client;
}

void set_hop(double new_hop) {
    if (new_hop < 0) cerr << "ignoring invalid hop: " << new_hop << endl;
    else {
        internals::hop = new_hop;
        internals::hop_samples = new_hop;
        internals::hop_fract = fmod(new_hop, 1.0)*double(uint64_t(-1));
    }
}
void next_hop_samples(uint32_t n, double h) {
    internals::new_window_size = n;
    set_hop(h);
}
void next_hop_ratio(uint32_t n, double ratio) {
    internals::new_window_size = n;
    set_hop(n*ratio);
}
void next_hop_hz(uint32_t n, double hz) {
    internals::new_window_size = n;
    set_hop(internals::rate/hz);
}
void set_process_fn(func_t fn) {
    internals::fptr = fn;
}
void set_time(double secs) {
    if (secs < 0)
        cerr << "negative time is not supported, ignoring call to set_time(" << secs << ")" << endl;
    else {
        base_time += current_time_secs() - secs;
        internals::time_samples = uint64_t(secs*internals::rate);
        internals::time_fract = fmod(secs*internals::rate, 1.0)*double(uint64_t(-1));
    }
}
void set_num_channels(size_t in, size_t out) {
    lock_guard<mutex> data_lk(internals::data_mtx);
    internals::ainqueue.clear();
    internals::aqueue.clear();
    //TODO should probably clear atmp too
    for (size_t i = in; i < internals::new_nch_in; i++) {
        jack_port_unregister(internals::client, internals::in_ports.back());
        internals::in_ports.pop_back();
    }
    for (size_t i = internals::new_nch_in; i < in; i++) {
        string name = "in" + to_string(i);
        internals::in_ports.push_back(jack_port_register(internals::client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0));
    }
    for (size_t i = out; i < internals::new_nch_out; i++) {
        jack_port_unregister(internals::client, internals::out_ports.back());
        internals::out_ports.pop_back();
    }
    for (size_t i = internals::new_nch_out; i < out; i++) {
        string name = "out" + to_string(i);
        internals::out_ports.push_back(jack_port_register(internals::client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0));
    }
    internals::new_nch_in = in;
    internals::new_nch_out = out;
}
void skip_to_now() {
    lock_guard<mutex> data_lk(internals::data_mtx);
    internals::ainqueue.resize(internals::nch_in*(internals::computed_hop+internals::window_size));
    internals::aqueue.resize(0);
}
