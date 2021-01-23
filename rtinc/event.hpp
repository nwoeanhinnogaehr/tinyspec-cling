#include <unordered_map>
#include <map>
#include <queue>
#include <optional>

struct Event {
    // NOTE: the start_time of an event should NEVER be updated without also updating evq
    double start_time = 0;
    function<WaveBuf()> gen_fn;
    function<void()> exec_fn;
    friend bool operator<(const Event &l, const Event &r) {
        return l.start_time < r.start_time;
    }
};
using Evref = uint64_t;
namespace internals {
    extern Evref next_evref;
    extern unordered_map<Evref, Event> evtab;
    extern unordered_map<string, Evref> nametab;

    // map from start_time to event
    // invariant: each Evref should only appear at most once
    extern multimap<double, Evref> evq; 

    extern double rate;
    extern optional<Evref> current_event;
}

Evref new_evref() { return internals::next_evref++; }

long double operator ""_hz(long double hz) { return internals::rate/hz; }
long double operator ""_sec(long double sec) { return internals::rate*sec; }
unsigned long long operator ""_hz(unsigned long long hz) { return internals::rate/hz; }
unsigned long long operator ""_sec(unsigned long long sec) { return internals::rate*sec; }

function<void(Event& ev)> At(double n) { return [=](Event& ev) { ev.start_time = n; }; }
function<void(Event& ev)> In(double n) { return [=](Event& ev) { ev.start_time = n + now(); }; }
function<void(Event& ev)> GenFn(function<WaveBuf()> fn) { return [=](Event& ev) { ev.gen_fn = fn; ev.exec_fn = {}; }; }
function<void(Event& ev)> Fn(function<void()> fn) { return [=](Event& ev) { ev.exec_fn = fn; ev.gen_fn = {}; }; }

void apply_params(Event &) {}
template <typename T1, typename...T>
void apply_params(Event &ev, T1 p1, T... p) {
    p1(ev);
    apply_params(ev, p...);
}

template <typename...T>
Evref event(Evref ref, T... params) {
    using namespace internals;
    Event &ev = evtab[ref];

    // erase any old versions of this from the queue
    auto range = evq.equal_range(ev.start_time);
    for (auto i = range.first; i != range.second; ++i)
        if ((*i).second == ref) {
            evq.erase(i);
            break;
        }

    apply_params(ev, params...);

    if (ev.start_time < now()) {
        cerr << "event scheduled for the past, discarding..." << endl;
        return -1;
    }

    evq.emplace(ev.start_time, ref);
    return ref;
}

template <typename...T>
Evref next_event(T... params) {
    return event(internals::current_event.value(), params...);
}

template <typename...T>
Evref event(string name, T... params) {
    auto it = internals::nametab.find(name);
    Evref ref;
    if (it == internals::nametab.end()) {
        ref = new_evref();
        internals::nametab[name] = ref;
    } else
        ref = (*it).second;
    return event(ref, params...);
}

void cancel(Evref ref) {
    using namespace internals;
    Event &ev = evtab[ref];
    auto range = evq.equal_range(ev.start_time);
    for (auto i = range.first; i != range.second; ++i)
        if ((*i).second == ref) {
            evq.erase(i);
            break;
        }
}
void cancel(string name) {
    auto it = internals::nametab.find(name);
    if (it != internals::nametab.end()) {
        cancel((*it).second);
    }
}

void cancel_all() {
    internals::evq.clear();
    //TODO: might also consider clearing the tables?
}
