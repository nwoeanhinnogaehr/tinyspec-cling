// TODO reorder args

namespace internals {
    extern jack_client_t *client;
}

#define CONNECT_MIN 0
#define CONNECT_MAX 1
#define CONNECT_ALL 2

struct PortPattern {
    string regex;
    size_t idx_s, idx_t;
    unsigned long flags;
};
size_t parse_int_or(string s, size_t otherwise) {
    if (s.size() == 0)
        return otherwise;
    try { return stoi(s); }
    catch (...) { cerr << "failed to parse \"" << s << "\" as integer" << endl; }
    return otherwise;
}
PortPattern parse_port_pattern(string filter) {
    PortPattern p;
    p.regex = ":.*";
    p.idx_s = 0;
    p.idx_t = -1;
    p.flags = JackPortIsOutput | JackPortIsInput; 
    if (filter.size() == 0);
    else if (filter[0] == ':') {
        p.regex = filter;
    } else {
        size_t range_idx = 1;
        if (filter[0] == 'i')
            p.flags = JackPortIsInput;
        else if (filter[0] == 'o')
            p.flags = JackPortIsOutput;
        else
            range_idx = 0;
        p.regex = ":.*";
        int idx = filter.find(',');
        if (idx == -1) {
            p.idx_s = parse_int_or(filter.substr(range_idx), 0);
            p.idx_t = parse_int_or(filter.substr(range_idx), -2) + 1;
        } else {
            p.idx_s = parse_int_or(filter.substr(range_idx, idx), 0);
            p.idx_t = parse_int_or(filter.substr(idx+1), -1);
        }
    }
    return p;
}

void dis_connect_impl(bool con, string client_a, string client_b, string filter_a, string filter_b, int mode) {
    const char* what = con ? "connect" : "disconnect";
    auto fn = con ? jack_connect : jack_disconnect;
    PortPattern a_ptn = parse_port_pattern(filter_a);
    PortPattern b_ptn = parse_port_pattern(filter_b);
    const char** ports_a_in = jack_get_ports(internals::client, (client_a + a_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    const char** ports_a_out = jack_get_ports(internals::client, (client_a + a_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    const char** ports_b_in = jack_get_ports(internals::client, (client_b + b_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    const char** ports_b_out = jack_get_ports(internals::client, (client_b + b_ptn.regex).c_str(),
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    size_t ports_a_in_len = 0; for (int i = 0; ports_a_in && ports_a_in[i]; i++) ports_a_in_len++;
    size_t ports_a_in_len_x = min(ports_a_in_len, a_ptn.idx_t-a_ptn.idx_s);
    size_t ports_a_out_len = 0; for (int i = 0; ports_a_out && ports_a_out[i]; i++) ports_a_out_len++;
    size_t ports_a_out_len_x = min(ports_a_out_len, a_ptn.idx_t-a_ptn.idx_s);
    size_t ports_b_in_len = 0; for (int i = 0; ports_b_in && ports_b_in[i]; i++) ports_b_in_len++;
    size_t ports_b_in_len_x = min(ports_b_in_len, b_ptn.idx_t-b_ptn.idx_s);
    size_t ports_b_out_len = 0; for (int i = 0; ports_b_out && ports_b_out[i]; i++) ports_b_out_len++;
    size_t ports_b_out_len_x = min(ports_b_out_len, b_ptn.idx_t-b_ptn.idx_s);
    auto lenfn = [=](size_t a, size_t b) { return mode ? max(a, b) : min(a, b); };
    int count = 0;
    if ((a_ptn.flags & JackPortIsInput) && (b_ptn.flags & JackPortIsOutput)
            && ports_b_out_len_x && ports_a_in_len_x)
        for (size_t i = 0; i < lenfn(ports_a_in_len_x, ports_b_out_len_x); i++) {
            for (size_t j = i; j < i + (mode == CONNECT_ALL ? lenfn(ports_a_in_len_x, ports_b_out_len_x) : 1); j++) {
                fn(internals::client, ports_b_out[(b_ptn.idx_s+i%ports_b_out_len_x)%ports_b_out_len],
                        ports_a_in[(a_ptn.idx_s+j%ports_a_in_len_x)%ports_a_in_len]);
                cerr << what << " " << ports_b_out[(b_ptn.idx_s+i%ports_b_out_len_x)%ports_b_out_len]
                    << " and " << ports_a_in[(a_ptn.idx_s+j%ports_a_in_len_x)%ports_a_in_len] << endl;
                count++;
            }
        }
    if ((a_ptn.flags & JackPortIsOutput) && (b_ptn.flags & JackPortIsInput)
            && ports_a_out_len_x && ports_b_in_len_x)
        for (size_t i = 0; i < lenfn(ports_a_out_len_x, ports_b_in_len_x); i++) {
            for (size_t j = i; j < i + (mode == CONNECT_ALL ? lenfn(ports_a_in_len_x, ports_b_out_len_x) : 1); j++) {
                fn(internals::client, ports_a_out[(a_ptn.idx_s+i%ports_a_out_len_x)%ports_a_out_len],
                        ports_b_in[(b_ptn.idx_s+j%ports_b_in_len_x)%ports_b_in_len]);
                cerr << what << " " << ports_a_out[(a_ptn.idx_s+i%ports_a_out_len_x)%ports_a_out_len]
                    << " and " << ports_b_in[(b_ptn.idx_s+j%ports_b_in_len_x)%ports_b_in_len] << endl;
                count++;
            }
        }
    jack_free(ports_a_in); jack_free(ports_b_in); jack_free(ports_a_out); jack_free(ports_b_out);
    if (count == 0)
        cerr << what << ": no matching ports" << endl;
}
void disconnect(string client_a, string client_b, string filter_a, string filter_b, int mode) {
    dis_connect_impl(false, client_a, client_b, filter_a, filter_b, mode);
}
void connect(string client_a, string client_b, string filter_a, string filter_b, int mode) {
    dis_connect_impl(true, client_a, client_b, filter_a, filter_b, mode);
}
