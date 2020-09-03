#include <complex>
using cplx = std::complex<double>;
using FFTBuf = Buf<cplx>;
void frft(FFTBuf &in, FFTBuf &out, double exponent);
