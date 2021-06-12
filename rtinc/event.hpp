#include <unordered_map>
#include <map>
#include <queue>
#include <optional>

struct Time {
    uint64_t integral;
    uint64_t fractional;
    Time(double samp) : integral(uint64_t(samp)), fractional(fmod(samp, 1.0)*uint64_t(-1)) {}
    Time(uint64_t i, uint64_t f) : integral(i), fractional(f) {}
    double samples() { return integral + fractional/double(uint64_t(-1)); }
    double secs() { return samples()/sample_rate(); }
    Time operator+(const Time other) const {
        uint64_t res_unused;
        return Time(
            integral + other.integral + __builtin_add_overflow(fractional, other.fractional, &res_unused),
            fractional + other.fractional
        );
    }
    friend bool operator<(const Time &l, const Time &r) {
        return l.integral == r.integral ?
            l.fractional < r.fractional :
            l.integral < r.integral;
    }
    template<typename T>
    static Time hz(T hz);
    template<typename T>
    static Time sec(T sec);
    template<typename T>
    static Time ms(T ms);
};


using EventTypeId = uint64_t;
using EventId = uint64_t;
struct EventType {
    string name;
    EventTypeId id;
    function<WaveBuf()> gen_fn;
    function<void()> exec_fn;
};
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
    extern Time time_now;
}
Time operator ""_hz(long double hz) { return internals::rate/hz; }
Time operator ""_sec(long double sec) { return internals::rate*sec; }
Time operator ""_ms(long double ms) { return internals::rate/1000*ms; }
Time operator ""_hz(unsigned long long hz) { return internals::rate/hz; }
Time operator ""_sec(unsigned long long sec) { return internals::rate*sec; }
Time operator ""_ms(unsigned long long ms) { return internals::rate/1000*ms; }

template<typename T>
Time Time::hz(T hz) { return internals::rate/hz; }
template<typename T>
Time Time::sec(T sec) { return internals::rate*sec; }
template<typename T>
Time Time::ms(T ms) { return internals::rate/1000*ms; }

EventTypeId new_event_type_id() { return internals::next_type_id++; }
EventTypeId new_event_id() { return internals::next_event_id++; }
struct EventTypeRef {
    uint64_t id;
    EventTypeRef run(function<void()> f) {
        EventType &ty = internals::type_map[id];
        ty.id = id;
        ty.exec_fn = f;
        return *this;
    }
    EventTypeRef snd(function<WaveBuf()> f) {
        EventType &ty = internals::type_map[id];
        ty.id = id;
        ty.gen_fn = f;
        return *this;
    }
    EventTypeRef in(Time t) {
        return this->at(t + internals::time_now);
    }
    EventTypeRef at(Time t) {
        EventType &ty = internals::type_map[id];
        ty.id = id;
        Event ev;
        ev.start_time = t;
        ev.type_id = id;
        ev.id = new_event_id();
        internals::event_map[ev.id] = ev;
        internals::event_queue.emplace(ev.start_time, ev.id);
        return *this;
    }
    EventTypeRef cancel() {
        using namespace internals;
        for (auto it = event_queue.begin(); it != event_queue.end();) {
            Event& ev = event_map[(*it).second];
            if (ev.type_id == id) {
                event_map.erase((*it).second);
                it = event_queue.erase(it);
            } else ++it;
        }
        return *this;
    }
};

EventTypeRef self() {
    return EventTypeRef {*internals::current_event_type};
}
EventTypeRef fn(EventTypeId type) {
    return EventTypeRef { type };
}
EventTypeRef fn(string name) {
    auto it = internals::name_map.find(name);
    EventTypeId type_id;
    if (it == internals::name_map.end()) {
        type_id = new_event_type_id();
        internals::name_map[name] = type_id;
    } else
        type_id = (*it).second;
    return fn(type_id);
}
EventTypeRef def_fn(string name) {
    EventTypeRef ev = fn(name);
    ev.cancel();
    return ev;
}
EventTypeRef def_fn(EventTypeId type) {
    EventTypeRef ev = fn(type);
    ev.cancel();
    return ev;
}
void cancel_all() {
    internals::event_queue.clear();
    internals::event_map.clear();
}
