#include<memory>
#ifndef OSC_H
namespace oscpkt {
    class Message;
}
struct RecvMsg {
    void *ar;
    oscpkt::Message *msg;
    RecvMsg(void *ar, oscpkt::Message *msg) : ar(ar), msg(msg) { }
    ~RecvMsg();
};
#endif
void _osc_send(const std::string &address, int port, double t, oscpkt::Message *msg);
oscpkt::Message *_osc_new_msg(const std::string& path);
void _osc_push(oscpkt::Message *m, int32_t v);
void _osc_push(oscpkt::Message *m, int64_t v);
void _osc_push(oscpkt::Message *m, float v);
void _osc_push(oscpkt::Message *m, double v);
void _osc_push(oscpkt::Message *m, const std::string &v);
void _osc_push_all(oscpkt::Message *) { }
template<typename T1, typename...T>
void _osc_push_all(oscpkt::Message *msg, T1 param1, T...params) {
    _osc_push(msg, param1);
    _osc_push_all(msg, params...);
}

template<typename...T>
void osc_send(const std::string &address, int port, double t, const std::string &path, T...params) {
    oscpkt::Message *msg = _osc_new_msg(path);
    _osc_push_all(msg, params...);
    _osc_send(address, port, t, msg);
}

bool _osc_pop(RecvMsg &m, int32_t &v);
bool _osc_pop(RecvMsg &m, int64_t &v);
bool _osc_pop(RecvMsg &m, float &v);
bool _osc_pop(RecvMsg &m, double &v);
bool _osc_pop(RecvMsg &m, std::string &v);

bool osc_get(RecvMsg&) { return true; }
template<typename T1, typename...T>
bool osc_get(RecvMsg &msg, T1 &v1, T&... v) {
    return _osc_pop(msg, v1) && osc_get(msg, v...);
}
std::vector<std::shared_ptr<RecvMsg>> osc_recv(int port, double t, const std::string &path);
