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
    Buf(const Buf<T> &other) { *this = other; }
    Buf(Buf<T> &&other) {
        data = other.data;
        size = other.size;
        num_channels = other.num_channels;
        other.data = nullptr;
        other.size = other.num_channels = 0;
    }
    ~Buf() { if (data) fftw_free(data); }

    void resize(size_t num_channels, size_t size) {
        if (this->size*this->num_channels != size*num_channels) {
            if (data) fftw_free(data);
            data = (T*) fftw_malloc(sizeof(T)*num_channels*size);
        }
        this->size = size;
        this->num_channels = num_channels;
        *this = T{};
    }
    void operator=(const Buf<T> &other) {
        resize(other.num_channels, other.size);
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = other.data[i];
    }
    void operator=(T scalar) {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = scalar;
    }
    void operator+=(T scalar) { apply([=](T a){ return a+scalar; }); }
    void operator-=(T scalar) { apply([=](T a){ return a-scalar; }); }
    void operator*=(T scalar) { apply([=](T a){ return a*scalar; }); }
    void operator/=(T scalar) { apply([=](T a){ return a/scalar; }); }
    void apply(std::function<T(const T&)> fn) {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = fn(data[i]);
    }
    void operator+=(const Buf<T> &other) { apply2(other, [=](T a, T b){ return a+b; }); }
    void operator-=(const Buf<T> &other) { apply2(other, [=](T a, T b){ return a-b; }); }
    void operator*=(const Buf<T> &other) { apply2(other, [=](T a, T b){ return a*b; }); }
    void operator/=(const Buf<T> &other) { apply2(other, [=](T a, T b){ return a/b; }); }
    void apply2(const Buf<T> &two, std::function<T(const T&, const T&)> fn) {
        for (size_t i = 0; i < size*num_channels; i++)
            data[i] = fn(data[i], two.data[i]);
    }
    Buf<T> operator+(T scalar) { return map([=](T a){ return a+scalar; }); }
    Buf<T> operator-(T scalar) { return map([=](T a){ return a-scalar; }); }
    Buf<T> operator*(T scalar) { return map([=](T a){ return a*scalar; }); }
    Buf<T> operator/(T scalar) { return map([=](T a){ return a/scalar; }); }
    template<typename U> Buf<U> map(std::function<U(T)> fn) {
        Buf<U> out(num_channels, size);
        for (size_t i = 0; i < size*num_channels; i++)
            out.data[i] = fn(data[i]);
        return out;
    }
    template<typename U> Buf<U> map(U fn(const T&)) {
        Buf<U> out(num_channels, size);
        for (size_t i = 0; i < size*num_channels; i++)
            out.data[i] = fn(data[i]);
        return out;
    }
    Buf<T> operator+(const Buf<T> &other) { return map2(other, [=](T a, T b){ return a+b; }); }
    Buf<T> operator-(const Buf<T> &other) { return map2(other, [=](T a, T b){ return a-b; }); }
    Buf<T> operator*(const Buf<T> &other) { return map2(other, [=](T a, T b){ return a*b; }); }
    Buf<T> operator/(const Buf<T> &other) { return map2(other, [=](T a, T b){ return a/b; }); }
    template<typename U, typename V> Buf<U> map2(const Buf<V> &two, std::function<U(const T&, const V&)> fn) {
        Buf<U> out(num_channels, size);
        for (size_t i = 0; i < size*num_channels; i++)
            out.data[i] = fn(data[i], two.data[i]);
        return out;
    }
    void apply(std::function<T(const T&,size_t,size_t)> fn) {
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size; j++)
                data[i*size + j] = fn(data[i*size + j], i, j);
    }
    void iterate(std::function<void(size_t,size_t)> fn) {
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size; j++)
                fn(i, j);
    }
    void generate(std::function<T(size_t,size_t)> fn) {
        for (size_t i = 0; i < num_channels; i++)
            for (size_t j = 0; j < size; j++)
                data[i*size + j] = fn(i, j);
    }
    T* operator[](size_t index) { return data + index*size; }
};
using WaveBuf = Buf<double>;
