//PV = Phase Vocoder
struct PVBin {
    double amp, freq, last_phase, phase_sum;
    PVBin()
        : amp(0), freq(0), last_phase(0), phase_sum(0) { }
    PVBin(double amp, double freq)
        : amp(amp), freq(freq), last_phase(0), phase_sum(0) { }
    PVBin(double amp, double freq, double last_phase, double phase_sum)
        : amp(amp), freq(freq), last_phase(last_phase), phase_sum(phase_sum) { }
};
using PVBuf = Buf<PVBin>;

namespace internals {
    extern double hop;
}
struct PhaseVocoder : Buf<PVBin> {
    FFTBuf fft;
    bool global_hop;
    double user_hop;
    PhaseVocoder() : Buf() { use_global_hop(); }
    PhaseVocoder(size_t num_channels, size_t size) : Buf(num_channels, size) { use_global_hop(); }
    //void fill_from(PhaseVocoder &other) {
        //global_hop = other.global_hop;
        //user_hop = other.user_hop;
        //Buf::fill_from(other);
    //}
    void use_global_hop() { global_hop = true; }
    void set_hop(double new_hop) {
        user_hop = new_hop;
        global_hop = false;
    }
    double hop() {
        if (global_hop) return internals::hop;
        else return user_hop;
    }
    void analyze(WaveBuf &other);
    void synthesize(WaveBuf &other);
    void analyze(FFTBuf &other);
    void synthesize(FFTBuf &other);
    double phase_to_frequency(size_t bin, double phase_diff);
    double frequency_to_phase(double freq);
    void shift(std::function<double(double)> fn);
    void shift(std::function<void(WaveBuf&)> fn);
};
