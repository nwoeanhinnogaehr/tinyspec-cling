namespace oscpkt {
    class Message;
}
void _osc_sched(double t, oscpkt::Message *msg);
oscpkt::Message *_osc_new_msg(const std::string& path);
void _osc_push(oscpkt::Message *m, int32_t v);
void _osc_push(oscpkt::Message *m, int64_t v);
void _osc_push(oscpkt::Message *m, float v);
void _osc_push(oscpkt::Message *m, double v);
void _osc_push(oscpkt::Message *m, const std::string &v);
void _osc_push_all(oscpkt::Message *msg) { }
template<typename T1, typename...T>
void _osc_push_all(oscpkt::Message *msg, T1 param1, T...params) {
    _osc_push(msg, param1);
    _osc_push_all(msg, params...);
}

void osc_init(const std::string &address, int port);
template<typename...T>
void osc_send(double t, const std::string &path, T...params) {
    oscpkt::Message *msg = _osc_new_msg(path);
    _osc_push_all(msg, params...);
    _osc_sched(t, msg);
}
