#include <iostream>
#include <vector>
using namespace std;

namespace internals {
    extern string client_name;
    extern double rate;
}

#define RATE internals::rate
#define CLIENT_NAME internals::client_name

#include "buffer.hpp"
#include "control.hpp"
#include "connect.hpp"
#include "osc.hpp"
#include "fft.hpp"
#include "windowfn.hpp"
#include "pv.hpp"
