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
struct Event {
    EventTypeId type_id = -1;
    EventId id = -1;
    Time start_time = 0;
    function<void()> run_fn;
    function<WaveBuf()> snd_fn;
    friend bool operator<(const Event &l, const Event &r) {
        return l.start_time < r.start_time;
    }
};
namespace internals {
    extern EventTypeId next_type_id;
    extern EventId next_event_id;
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
template <typename ...T>
struct Def;
template <typename ...T>
struct EventRef {
    EventId evt_id;
    Def<T...> &def;

    Def<T...>& cancel() {
        using namespace internals;
        for (auto it = event_queue.begin(); it != event_queue.end();) {
            Event& ev = event_map[(*it).second];
            if (ev.id == evt_id) {
                event_map.erase((*it).second);
                break;
            } else ++it;
        }
        return def;
    }
    Def<T...>& snd_fn(function<WaveBuf(T...)> f) { return def.snd_fn(f); }
    Def<T...>& run_fn(function<void(T...)> f) { return def.run_fn(f); }
    EventRef in(Time t, T... args) { return def.in(t, args...); }
    EventRef at(Time t, T... args) { return def.at(t, args...); }
};
template<typename ...T>
struct Def {
    EventTypeId ty_id;
    function<WaveBuf(T...)> _snd_fn;
    function<void(T...)> _run_fn;

    Def() : ty_id(new_event_type_id()) { }

    Def& snd_fn(function<WaveBuf(T...)> f) {
        _snd_fn = f;
        return *this;
    }
    Def& run_fn(function<void(T...)> f) {
        _run_fn = f;
        return *this;
    }
    Def& cancel() {
        using namespace internals;
        for (auto it = event_queue.begin(); it != event_queue.end();) {
            Event& ev = event_map[(*it).second];
            if (ev.type_id == ty_id) {
                event_map.erase((*it).second);
                it = event_queue.erase(it);
            } else ++it;
        }
        return *this;
    }

    EventRef<T...> in(Time t, T... args) {
        return at(t + internals::time_now, args...);
    }
    EventRef<T...> at(Time t, T... args) {
        Event ev;
        ev.start_time = t;
        ev.type_id = ty_id;
        ev.id = new_event_id();
        if (_run_fn)
            ev.run_fn = bind(_run_fn, args...);
        if (_snd_fn)
            ev.snd_fn = bind(_snd_fn, args...);
        internals::event_map[ev.id] = ev;
        internals::event_queue.emplace(ev.start_time, ev.id);
        return EventRef<T...>{ev.id, *this};
    }
};
void cancel_all() {
    internals::event_queue.clear();
    internals::event_map.clear();
}
