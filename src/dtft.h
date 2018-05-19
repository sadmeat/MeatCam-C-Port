#include <complex>

std::complex<double> fftSamplePoints[FFT_SIZE][RECORD_BUFFER_SIZE];

void buildSamplePoints() {
    const double pi = std::acos(-1);
    const std::complex<double> i(0, 1);

    for(int k=0; k<FFT_SIZE; k++) {
        for(int j=0; j<RECORD_BUFFER_SIZE; j++) {
            fftSamplePoints[k][j] +=  std::exp(-pi*0.25*((double)j*k/FFT_SIZE)*i);
        }
    }
}

void dtft(short* signal, unsigned short* out) {
    for(int k=0; k<FFT_SIZE; k++) {
        std::complex<double> sum(0, 0);

        for(int j=0; j<RECORD_BUFFER_SIZE; j++)
            sum +=  (double) signal[j] * fftSamplePoints[k][j];

        out[k] = (unsigned short) std::abs(sum);
    }
}
