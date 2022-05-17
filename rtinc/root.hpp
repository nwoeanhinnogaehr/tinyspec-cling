#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

#define DISABLE_PTRCHECK __attribute__((annotate("__cling__ptrcheck(off)")))

#include "buffer.hpp"
#include "control.hpp"
#include "connect.hpp"
#include "osc.hpp"
#include "fft.hpp"
#include "windowfn.hpp"
//#include "pv.hpp"
#include "event.hpp"
