void next_hop_ratio(uint32_t n, double hop_ratio=0.25);
void next_hop_samples(uint32_t n, double h);
void next_hop_hz(uint32_t n, double hz);
void set_num_channels(size_t in, size_t out);
void skip_to_now();
void set_process_fn(function<void(WaveBuf&, WaveBuf&, double)> fn);
void set_time(double secs);
