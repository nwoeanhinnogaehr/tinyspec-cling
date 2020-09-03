#pragma once

#include <fftw3.h>
#include <functional>
#include <cassert>

template <typename T>
struct Buf {
    T *data = nullptr;
    size_t size = 0;
    size_t num_channels = 0;
    Buf() {}
    Buf(size_t num_channels, size_t size) { resize(num_channels, size); }
    ~Buf() { if (data) free(data); }
    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            this->size = size;
            this->num_channels = num_channels;
            if (data) fftw_free(data);
            data = (T*) fftw_malloc(sizeof(T)*num_channels*size);
            zero();
        }
    }
    template <typename U>
    void fill_from(Buf<U> &other, std::function<T(U)> mix=[](U x){ return T(x); }) {
        resize(other.num_channels, other.size);
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size; j++)
                data[i*size + j] = mix(other[i][j]);
    }
    void zero() {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = {};
    }
    T* operator[](int index) { return data + index*size; }
};
using WaveBuf = Buf<double>;
