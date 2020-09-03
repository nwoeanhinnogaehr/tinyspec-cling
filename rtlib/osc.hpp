#include<list>
#include<unordered_map>
#include"oscpkt/oscpkt.hh"
#include"oscpkt/udp.hh"
using namespace oscpkt;
using namespace std;
bool debug_osc = false;
#define OSC_H

// OSC stuff
// If this looks ridiculous it's because I had to work around a very odd cling bug.
// Eventually I will write my own OSC lib to avoid this mess.
unordered_map<string, UdpSocket> socks;
struct timeval init_time;
double base_time = 0;
uint64_t to_timestamp(double t) {
    return ((init_time.tv_sec + 2208988800u + uint64_t(t + base_time)) << 32)
        + 4294.967296*(init_time.tv_usec + fmod(t + base_time, 1.0)*1000000);
}
void _osc_send(const string &address, int port, double t, Message *msg) {
    auto res = socks.emplace(address+":"+to_string(port), UdpSocket());
    UdpSocket &sock = res.first->second;
    if (res.second) {
        sock.connectTo(address, port);
        if (!sock.isOk()) cerr << "Error connecting: " << sock.errorMessage() << endl;
        else cerr << "Connect ok!" << endl;
    }
    uint64_t timestamp = to_timestamp(t);
    PacketWriter pw;
    pw.startBundle(TimeTag(timestamp)).addMessage(*msg).endBundle();
    if (!sock.sendPacket(pw.packetData(), pw.packetSize()))
        cerr << "Send error: " << sock.errorMessage() << endl;
    delete msg;
}
Message *_osc_new_msg(const string &path) { return new oscpkt::Message(path); }
void _osc_push(Message *m, int32_t v) { m->pushInt32(v); }
void _osc_push(Message *m, int64_t v) { m->pushInt64(v); }
void _osc_push(Message *m, float v) { m->pushFloat(v); }
void _osc_push(Message *m, double v) { m->pushDouble(v); }
void _osc_push(Message *m, const string &v) { m->pushStr(v); }

RecvMsg::~RecvMsg() {
    delete (Message::ArgReader*) ar;
    delete msg;
}
bool _osc_pop(RecvMsg &m, int32_t &v) { return ((Message::ArgReader*)m.ar)->popInt32(v); }
bool _osc_pop(RecvMsg &m, int64_t &v) { return ((Message::ArgReader*)m.ar)->popInt64(v); }
bool _osc_pop(RecvMsg &m, float &v) { return ((Message::ArgReader*)m.ar)->popFloat(v); }
bool _osc_pop(RecvMsg &m, double &v) { return ((Message::ArgReader*)m.ar)->popDouble(v); }
bool _osc_pop(RecvMsg &m, string &v) { return ((Message::ArgReader*)m.ar)->popStr(v); }

struct Server {
    UdpSocket sock;
    list<Message> queue;
};
unordered_map<int, Server> servers;
vector<shared_ptr<RecvMsg>> osc_recv(int port, double t, const string &path) {
    uint64_t timestamp = to_timestamp(t);
    vector<shared_ptr<RecvMsg>> out;
    auto res = servers.emplace(port, Server());
    UdpSocket &sock = res.first->second.sock;
    auto &queue = res.first->second.queue;
    if (res.second) {
        sock.bindTo(port);
        if (!sock.isOk()) cerr << "Error binding: " << sock.errorMessage() << endl;
        else cerr << "OSC Server started on port " << std::to_string(port) << endl;
    }
    PacketReader pr;
    while (sock.receiveNextPacket(0)) {
        pr.init(sock.packetData(), sock.packetSize());
        oscpkt::Message *msg;
        while (pr.isOk() && (msg = pr.popMessage()))
            queue.emplace_back(*msg);
    }
    for (auto it = queue.begin(); it != queue.end();) {
        Message &msg = *it;
        if (debug_osc)
            cout << msg << endl;
        if (msg.match(path) && msg.timeTag() < timestamp) {
            Message *copy = new Message;
            *copy = msg;
            auto ar = copy->match(path);
            auto *arcopy = new Message::ArgReader(ar);
            out.push_back(make_shared<RecvMsg>(arcopy, copy));
            it = queue.erase(it);
        }
        else ++it;
    }
    return out;
}
