#include <unordered_map>
#include <map>
#include <queue>
#include <optional>

struct Time {
    uint64_t integral;
    uint64_t fractional;
    Time(double samp) : integral(uint64_t(samp)), fractional(fmod(samp, 1.0)*uint64_t(-1)) {}
    double samples() { return integral + fractional/double(uint64_t(-1)); }
    double secs() { return samples()/sample_rate(); }
    friend bool operator<(const Time &l, const Time &r) {
        return l.integral == r.integral ?
            l.fractional < r.fractional :
            l.integral < r.integral;
    }
};

using EventTypeId = uint64_t;
struct EventType {
    string name;
    EventTypeId id;
    function<WaveBuf()> gen_fn;
    function<void()> exec_fn;
};
using EventId = uint64_t;
struct Event {
    EventTypeId type_id = -1;
    EventId id = -1;
    Time start_time = 0;
    friend bool operator<(const Event &l, const Event &r) {
        return l.start_time < r.start_time;
    }
};
namespace internals {
    extern EventTypeId next_type_id;
    extern EventId next_event_id;
    extern unordered_map<EventTypeId, EventType> type_map;
    extern unordered_map<string, EventTypeId> name_map;
    extern unordered_map<EventId, Event> event_map;
    extern optional<EventTypeId> current_event_type;
    extern optional<EventId> current_event;
    extern multimap<Time, EventId> event_queue; // map from start_time to event

    extern double rate;
}

EventTypeId new_event_type_id() { return internals::next_type_id++; }
EventTypeId new_event_id() { return internals::next_event_id++; }

long double operator ""_hz(long double hz) { return internals::rate/hz; }
long double operator ""_sec(long double sec) { return internals::rate*sec; }
long double operator ""_ms(long double ms) { return internals::rate/1000*ms; }
unsigned long long operator ""_hz(unsigned long long hz) { return internals::rate/hz; }
unsigned long long operator ""_sec(unsigned long long sec) { return internals::rate*sec; }
unsigned long long operator ""_ms(unsigned long long ms) { return internals::rate/1000*ms; }

function<void(EventType& ty, Event& ev)> At(double n) { return [=](EventType& ty, Event& ev) {
    ev.start_time = n;
    ev.type_id = ty.id;
    ev.id = new_event_id();
}; }
function<void(EventType& ty, Event& ev)> In(double n) { return [=](EventType& ty, Event& ev) {
    ev.start_time = n + now();
    ev.type_id = ty.id;
    ev.id = new_event_id();
}; }
function<void(EventType& ty, Event& ev)> GenFn(function<WaveBuf()> fn) { return [=](EventType& ty, Event& ev) {
    ty.gen_fn = fn;
    ty.exec_fn = {};
}; }
function<void(EventType& ty, Event& ev)> Fn(function<void()> fn) { return [=](EventType& ty, Event& ev) {
    ty.exec_fn = fn;
    ty.gen_fn = {};
}; }

void apply_params(EventType& ty, Event &) {}
template <typename T1, typename...T>
void apply_params(EventType& ty, Event &ev, T1 p1, T... p) {
    p1(ty, ev);
    apply_params(ty, ev, p...);
}

template <typename...T>
void event(EventTypeId type_id, T... params) {
    using namespace internals;
    EventType &ty = type_map[type_id];
    ty.id = type_id;
    Event ev;

    apply_params(ty, ev, params...);

    if (ev.id != uint64_t(-1)) { // if a start time was specified, schedule it
        event_map[ev.id] = ev;
        event_queue.emplace(ev.start_time, ev.id);
    }
}

template <typename...T>
void this_event(T... params) {
    event(internals::current_event_type.value(), params...);
}

template <typename...T>
void event(string name, T... params) {
    auto it = internals::name_map.find(name);
    EventTypeId type_id;
    if (it == internals::name_map.end()) {
        type_id = new_event_type_id();
        internals::name_map[name] = type_id;
    } else
        type_id = (*it).second;
    event(type_id, params...);
}

void cancel(EventTypeId type_id) {
    using namespace internals;
    for (auto it = event_queue.begin(); it != event_queue.end();) {
        Event& ev = event_map[(*it).second];
        if (ev.type_id == type_id) {
            event_map.erase((*it).second);
            it = event_queue.erase(it);
        } else ++it;
    }
}
void cancel(string name) {
    auto it = internals::name_map.find(name);
    if (it != internals::name_map.end()) {
        cancel((*it).second);
    }
}

void cancel_all() {
    internals::event_queue.clear();
    internals::event_map.clear();
}

template <typename...T>
void def_event(string name, T... params) {
    cancel(name);
    event(name, params...);
}
