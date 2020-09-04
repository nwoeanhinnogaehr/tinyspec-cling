//PV = Phase Vocoder
struct PVBin {
    double amp, freq;
    PVBin() : amp(0), freq(0) { }
    PVBin(double amp, double freq) : amp(amp), freq(freq) { }
};
using PVBuf = Buf<PVBin>;

namespace internals {
    extern double hop;
}
struct PhaseVocoder {
    PVBuf data;
    Buf<double> last_phase;
    Buf<double> phase_sum;
    size_t size = 0;
    size_t num_channels = 0;
    FFTBuf fft;
    bool global_hop;
    double user_hop;
    PhaseVocoder() { use_global_hop(); }
    PhaseVocoder(size_t num_channels, size_t size) : PhaseVocoder() { resize(num_channels, size); }
    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            this->size = size;
            this->num_channels = num_channels;
            data.resize(num_channels, size);
            last_phase.resize(num_channels, size);
            phase_sum.resize(num_channels, size);
        }
    }
    void reset(bool last=true, bool sum=true) {
        if (last) last_phase.zero();
        if (sum) phase_sum.zero();
    }
    void fill_from(PhaseVocoder &other) {
        num_channels = other.num_channels;
        size = other.size;
        global_hop = other.global_hop;
        user_hop = other.user_hop;
        data.fill_from(other.data);
        last_phase.fill_from(other.last_phase);
        phase_sum.fill_from(other.phase_sum);
    }
    void use_global_hop() { global_hop = true; }
    void set_hop(double new_hop) {
        user_hop = new_hop;
        global_hop = false;
    }
    double hop() {
        if (global_hop) return internals::hop;
        else return user_hop;
    }
    PVBin* operator[](int index) { return data[index]; }
    void analyze(WaveBuf &other);
    void synthesize(WaveBuf &other);
    void analyze(FFTBuf &other);
    void synthesize(FFTBuf &other);
    double phase_to_frequency(size_t bin, double phase_diff);
    double frequency_to_phase(double freq);
    void shift(std::function<double(double)> fn);
    void shift(std::function<void(WaveBuf&)> fn);
};
