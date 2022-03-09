#pragma once

// TODO ifdef a scalar on the fft frequency because fft amplitude is much smaller on linux than on windows

#include <iostream>
#include <chrono>
namespace chrono = std::chrono;
#include <cmath> // std::abs
#include <limits> // numeric_limits<T>::infinity()
#include <mutex>
#include <complex>
#include <thread>
using std::complex;
#include <algorithm>
#include <numeric>

#ifdef WINDOWS
#define HAVE_PAR_ALGS
#endif

#ifdef HAVE_PAR_ALGS
#include <execution>
#endif

#include "AudioStreams/AudioStream.h"
#include "ShaderConfig.h" // AudioOptions

#include "ffts.h"

static const int VISUALIZER_BUFSIZE = 2 * 1024;
struct AudioData {
    float* audio_l;
    float* audio_r;
    float* freq_l;
    float* freq_r;
    std::mutex mtx;
};

// TODO wrap fft stuff/complexity into a class
// FFT computation library needs aligned memory
// TODO std::aligned_alloc is in c++17 apparently
#ifdef WINDOWS
#define Aligned_Alloc(align, sz) _aligned_malloc(sz, align)
#define Aligned_Free(ptr) _aligned_free(ptr)
#else
#define Aligned_Alloc(align, sz) aligned_alloc(align, sz)
#define Aligned_Free(ptr) free(ptr)
#endif

// sample rate of the audio stream
static const int SR = 48000;
// sample rate of audio given to FFT
static const int SRF = SR / 2;
// length of the system audio capture buffer (in frames)
static const int ABL = 512;
// number of system audio capture buffers to keep
static const int ABN = 16;
// total audio buffer length. length of all ABN buffers combined (in frames)
static const int TBL = ABL * ABN;
static const int FFTLEN = TBL / 2;
// length of visualizer 1D texture buffers. Number of frames of audio this module outputs.
static const int VL = VISUALIZER_BUFSIZE;

// I think the FFT looks best when it has 8192/48000, or 4096/24000, time granularity. However, I
// like the wave to be 96000hz just because then it fits on the screen nice. To have both an FFT on
// 24000hz data and to display 96000hz waveform data, I ask pulseaudio to output 48000hz and then
// resample accordingly. I've compared the 4096 sample FFT over 24000hz data to the 8192 sample FFT
// over 48000hz data and they are surprisingly similar. I couldn't tell a difference really. I've
// also compared resampling with ffmpeg to my current naive impl. Hard to notice a difference.

#ifdef HAVE_PAR_ALGS
// Cross correlation sync options
// store the n most recent buffers sent to the visualizer (stores contents of visualizer buffer).
static const int HISTORY_NUM_FRAMES = 50;
// search an interval of samples centered around the current read index (r) for the maximum cross correlation.
static const int HISTORY_SEARCH_RANGE = 128;
#else
static const int HISTORY_NUM_FRAMES = 9;
static const int HISTORY_SEARCH_RANGE = 32;
#endif

static const int HISTORY_SEARCH_GRANULARITY = 4;

// take into consideration the whole visualizer buffer
static const int HISTORY_BUFF_SIZE = VL;

// The performance cost for the cross correlations is roughly
// HISTORY_SEARCH_RANGE * HISTORY_NUM_FRAMES * HISTORY_BUFF_SIZE / HISTORY_SEARCH_GRANULARITY

// AudioProcess does not create AudioStream because ProceduralAudioStream must be created by AudioProcess's owner
template <typename ClockT, typename AudioStreamT>
class AudioProcess {
public:
    AudioProcess(AudioStreamT&, AudioOptions);
    ~AudioProcess();

    void step();
    void start() {
        while (!exit_audio_system_flag) {
            if (audio_system_paused) {
                step();
            }
            else {
                std::this_thread::sleep_for(chrono::milliseconds(500));
            }
        }
    }
    void exit_audio_system() {
        exit_audio_system_flag = true;
    }
    void pause_audio_system() {
        audio_system_paused = false;
    }
    void start_audio_system() {
        audio_system_paused = true;
    }
    AudioData& get_audio_data() {
        return audio_sink;
    }
    void set_audio_options(AudioOptions& ao) {
        xcorr_sync = ao.xcorr_sync;
        fft_sync = ao.fft_sync;
        wave_smoother = ao.wave_smooth;
        fft_smoother = ao.fft_smooth;
    }

private:
    bool audio_system_paused = true;
    bool exit_audio_system_flag = false;

    // Mixes the old audio buffer with the new audio buffer
    float wave_smoother;
    // Mixes the old fft buffer with the new fft buffer
    float fft_smoother;

    // The audio pointer in the circular buffer should be moved such that it travels only distances
    // that are whole multiples, not fractional multiples, of the audio wave's wavelength.
    // But how do we find the wavelength of the audio wave? If we try and look for the zero's
    // of the audio wave and note the distance between them, then we will not get an accurate
    // or consistent measure of the wavelength because the noise in the waveform will likely throw
    // our measure off.
    //
    // The wavelength of a waveform is related to the frequency by this equation
    // wavelength = velocity/frequency
    //
    // velocity = sample rate
    // wavelength = number of samples of one period of the wave
    // frequency = dominant frequency of the waveform
    bool fft_sync;

    // Increases similarity between successive frames of audio output by the AudioProcess
    bool xcorr_sync;

    typename ClockT::time_point next_time;
    int frame_id = 0;

    float* history_buff_l[HISTORY_NUM_FRAMES];
    float* history_buff_r[HISTORY_NUM_FRAMES];

    float* audio_buff_l;
    float* audio_buff_r;

    ffts_plan_t* fft_plan;
    complex<float>* fft_out_l;
    float* fft_in_l;
    complex<float>* fft_out_r;
    float* fft_in_r;
    float* fft_window;

    int writer;
    int reader_l;
    int reader_r;
    float freq_l;
    float freq_r;

    float channel_max_l;
    float channel_max_r;

    struct AudioData audio_sink;
    AudioStreamT& audio_stream;

    // Returns the bin holding the max frequency of the fft. we only consider the first 100 bins.
    static int max_bin(const complex<float>* f);
    static float max_frequency(const complex<float>* f);

    // Returns a multiple of freq such that mult * freq is close to thres
    static float get_harmonic_less_than(float freq, float thres);

    // Move p around the circular buffer by the amound and direction delta
    // tbl: total buffer length of the circular buffer
    static int move_index(int p, int delta, int tbl);
    static int dist_forward(int from, int to, int tbl);
    static int dist_backward(int from, int to, int tbl);

    static float mix(float x, float y, float m);

    // Adds the wavelength, computed by sample_rate / frequency, to r.
    // If w is within VL units in front of r, then adjust r so that this is no longer true.
    // w: index of the writer in the circular buffer
    // r: index of the reader in the circular buffer
    // freq: frequency to compute the wavelength with
    static int advance_index(int w, int r, float freq, int tbl);

    // Compute the dot product between a(t + a_offset) and b(b_size-t)
    static float reverse_dot_prod(const float* a, const float* b, int a_offset, int a_size, int b_size);

    static int cross_correlation_sync(const int w,
        const int r,
        const int dist,
        float* history_buff[HISTORY_NUM_FRAMES],
        const int frame_id,
        const float* buff);

    // converts number of seconds x to a time duration for the ClockT type
    static typename ClockT::duration dura(float x);
};

template <typename ClockT, typename AudioStreamT>
AudioProcess<ClockT, AudioStreamT>::AudioProcess(AudioStreamT& _audio_stream, AudioOptions audio_options)
    : audio_stream(_audio_stream), audio_sink() {
    if (audio_stream.get_sample_rate() != 48000) {
        std::cout << "The AudioProcess is meant to consume 48000hz audio but the given AudioStream "
            << "produces " << audio_stream.get_sample_rate() << "hz audio." << std::endl;
        exit(1);
    }
    if (audio_stream.get_max_buff_size() < ABL) {
        std::cout << "AudioProcess needs at least " << ABL << " frames per call to get_next_pcm"
            << " but the given AudioStream only provides " << audio_stream.get_max_buff_size()
            << "." << std::endl;
        exit(1);
    }

    xcorr_sync = audio_options.xcorr_sync;
    fft_sync = audio_options.fft_sync;
    wave_smoother = audio_options.wave_smooth;
    fft_smoother = audio_options.fft_smooth;

    // new float[x]() zero initializes for scalars
    audio_buff_l = new float[TBL]();
    audio_buff_r = new float[TBL]();

    for (int i = 0; i < HISTORY_NUM_FRAMES; ++i) {
        history_buff_l[i] = new float[HISTORY_BUFF_SIZE]();
        history_buff_r[i] = new float[HISTORY_BUFF_SIZE]();
    }

    audio_sink.audio_l = new float[VL]();
    audio_sink.audio_r = new float[VL]();
    audio_sink.freq_l = new float[VL]();
    audio_sink.freq_r = new float[VL]();

    const int N = FFTLEN;
    fft_plan = ffts_init_1d_real(N, FFTS_FORWARD);
    fft_in_l = (float*)Aligned_Alloc(32, N * sizeof(float));
    fft_in_r = (float*)Aligned_Alloc(32, N * sizeof(float));
    fft_out_l = (complex<float>*)Aligned_Alloc(32, (N / 2 + 1) * sizeof(complex<float>));
    fft_out_r = (complex<float>*)Aligned_Alloc(32, (N / 2 + 1) * sizeof(complex<float>));
    fft_window = new float[N];
    for (int i = 0; i < N; ++i)
        fft_window[i] = std::pow(sin(3.1415926f * i / N), 2);

    // Holds a harmonic frequency of the dominant frequency of the audio.
    freq_l = 60.f;
    freq_r = 60.f;

    // The index of the writer in the audio buffers
    writer = 0;
    reader_l = 0;
    reader_r = 0;

    channel_max_l = 1.f;
    channel_max_r = 1.f;
}

template <typename ClockT, typename AudioStreamT>
AudioProcess<ClockT, AudioStreamT>::~AudioProcess() {
    delete[] audio_sink.audio_l;
    delete[] audio_sink.audio_r;
    delete[] audio_sink.freq_l;
    delete[] audio_sink.freq_r;
    delete[] audio_buff_l;
    delete[] audio_buff_r;

    Aligned_Free(fft_in_l);
    Aligned_Free(fft_in_r);
    Aligned_Free(fft_out_l);
    Aligned_Free(fft_out_r);
    delete[] fft_window;

    for (int i = 0; i < HISTORY_NUM_FRAMES; ++i) {
        delete[] history_buff_l[i];
        delete[] history_buff_r[i];
    }

    ffts_free(fft_plan);
}

template <typename ClockT, typename AudioStreamT>
void AudioProcess<ClockT, AudioStreamT>::step() {
    audio_stream.get_next_pcm(audio_buff_l + writer, audio_buff_r + writer, ABL);
    writer = move_index(writer, ABL, TBL);

    const auto now_time = ClockT::now();
    // TODOFPS fps should be obtained from somewhere else
    if (now_time - next_time > chrono::milliseconds(144)) {
        next_time = now_time - chrono::milliseconds(1);
    }

    if (now_time > next_time) {
        // Downsample and window the audio
        for (int i = 0; i < FFTLEN; ++i) {
            fft_in_l[i] = audio_buff_l[(i * 2 + writer) % TBL] * fft_window[i];
            fft_in_r[i] = audio_buff_r[(i * 2 + writer) % TBL] * fft_window[i];
        }
        ffts_execute(fft_plan, fft_in_l, fft_out_l);
        ffts_execute(fft_plan, fft_in_r, fft_out_r);
        fft_out_l[0] = 0;
        fft_out_r[0] = 0;
        fft_out_l[1] = 0;
        fft_out_r[1] = 0;

        if (fft_sync) {
            freq_l = get_harmonic_less_than(max_frequency(fft_out_l), 80.f);
            freq_r = get_harmonic_less_than(max_frequency(fft_out_r), 80.f);
        }
        else {
            freq_l = 60.f;
            freq_r = 60.f;
        }

        // Get next read location in audio buffer
        reader_l = advance_index(writer, reader_l, freq_l, TBL);
        reader_r = advance_index(writer, reader_r, freq_r, TBL);
        if (xcorr_sync) {
            std::copy(audio_sink.audio_l, audio_sink.audio_l + VL, history_buff_l[frame_id % HISTORY_NUM_FRAMES]);
            std::copy(audio_sink.audio_r, audio_sink.audio_r + VL, history_buff_r[frame_id % HISTORY_NUM_FRAMES]);
            reader_l = cross_correlation_sync(writer, reader_l, HISTORY_SEARCH_RANGE, history_buff_l, frame_id, audio_buff_l);
            reader_r = cross_correlation_sync(writer, reader_r, HISTORY_SEARCH_RANGE, history_buff_r, frame_id, audio_buff_r);
        }

        float max_amplitude_l = std::numeric_limits<float>::min();
        float max_amplitude_r = std::numeric_limits<float>::min();
        for (int i = 0; i < VL; ++i) {
            float sample_l = audio_buff_l[(i + reader_l) % TBL];
            float sample_r = audio_buff_r[(i + reader_r) % TBL];
            if (std::abs(sample_l) > max_amplitude_l)
                max_amplitude_l = std::abs(sample_l);
            if (std::abs(sample_r) > max_amplitude_r)
                max_amplitude_r = std::abs(sample_r);
        }
        // Rescale with a delay so the rescaling is less obvious and unnatural looking
        channel_max_l = mix(channel_max_l, max_amplitude_l, .3f);
        channel_max_r = mix(channel_max_r, max_amplitude_r, .3f);

        audio_sink.mtx.lock();
        for (int i = 0; i < VL; ++i) {
            float sample_l = .66f * audio_buff_l[(i + reader_l) % TBL] / (channel_max_l + 0.0001f);
            float sample_r = .66f * audio_buff_r[(i + reader_r) % TBL] / (channel_max_r + 0.0001f);

            audio_sink.audio_l[i] = mix(audio_sink.audio_l[i], sample_l, wave_smoother);
            audio_sink.audio_r[i] = mix(audio_sink.audio_r[i], sample_r, wave_smoother);

            audio_sink.freq_l[i] = mix(audio_sink.freq_l[i], std::abs(fft_out_l[i]) / std::sqrt(float(FFTLEN)), fft_smoother);
            audio_sink.freq_r[i] = mix(audio_sink.freq_r[i], std::abs(fft_out_r[i]) / std::sqrt(float(FFTLEN)), fft_smoother);
        }

        // Smooth the fft a bit
        const int width = 2;
        for (int i = width; i < VL - width; ++i) {
            float sum_l = 0.f;
            float sum_r = 0.f;
            const float weight_sum = width * 1.5f + .5f;
            for (int j = -width; j <= width; ++j) {
                sum_l += (1.f - std::abs(j) / (2.f * width)) * audio_sink.freq_l[i + j];
                sum_r += (1.f - std::abs(j) / (2.f * width)) * audio_sink.freq_r[i + j];
            }
            audio_sink.freq_l[i] = sum_l / weight_sum;
            audio_sink.freq_r[i] = sum_r / weight_sum;
        }
        audio_sink.mtx.unlock();

        frame_id++;
        // TODOFPS
        next_time += dura(1 / 144.f);
    }
}

template <typename ClockT, typename AudioStreamT>
int AudioProcess<ClockT, AudioStreamT>::max_bin(const complex<float>* f) {
    float max_norm = 0.f;
    int max_i = 0;
    // catch frequencies from 5.86 to 586 (that is i * SRF / FFTLEN for i from 1 to 100)
    for (int i = 1; i < 100; ++i) {
        const float norm = std::norm(f[i]);
        if (norm > max_norm) {
            max_i = i;
            max_norm = norm;
        }
    }
    return max_i;
}

template <typename ClockT, typename AudioStreamT>
int AudioProcess<ClockT, AudioStreamT>::move_index(int p, int delta, int tbl) {
    p = (p + delta) % tbl;
    if (p < 0) {
        p += tbl;
    }
    return p;
}

template <typename ClockT, typename AudioStreamT>
int AudioProcess<ClockT, AudioStreamT>::dist_forward(int from, int to, int tbl) {
    int d = to - from;
    if (d < 0)
        d += tbl;
    return d;
}

template <typename ClockT, typename AudioStreamT>
int AudioProcess<ClockT, AudioStreamT>::dist_backward(int from, int to, int tbl) {
    return dist_forward(to, from, tbl);
}

template <typename ClockT, typename AudioStreamT>
float AudioProcess<ClockT, AudioStreamT>::max_frequency(const complex<float>* f) {
    // more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
    // https://ccrma.stanford.edu/~jos/sasp/Quadratic_Interpolation_Spectral_Peaks.html
    int k = max_bin(f);
    if (k == 0) {
        k = 1;
    }
    const float a = std::abs(f[k - 1]); // std:abs(complex) gives magnitudes
    const float b = std::abs(f[k]);
    const float g = std::abs(f[k + 1]);
    const float d = .5f * (a - g) / (a - 2 * b + g + 0.001f);
    const float kp = k + d;
    // dont let anything negative or close to zero through
    return std::max(kp * float(SRF) / float(FFTLEN), 10.f);
}

template <typename ClockT, typename AudioStreamT>
float AudioProcess<ClockT, AudioStreamT>::get_harmonic_less_than(float freq, float thres) {
    const float a = std::log2f(freq);
    const float b = std::log2f(thres);
    freq *= std::pow(2, int(std::floor(b - a)));
    if (!std::isnormal(freq))
        freq = 60.f;
    return freq;
}

template <typename ClockT, typename AudioStreamT>
float AudioProcess<ClockT, AudioStreamT>::mix(float x, float y, float m) {
    return (1.f - m) * x + (m)* y;
}

template <typename ClockT, typename AudioStreamT>
int AudioProcess<ClockT, AudioStreamT>::advance_index(int w, int r, float freq, int tbl) {
    int wave_len = int(SR / freq + .5f);

    r = move_index(r, wave_len, tbl);

    //if (int df = dist_forward(r, w, tbl); df < VL)
    //    cout << "reader too close: " << df << "\twave_len: " << wave_len << endl;

    // skip past the discontinuity
    while (dist_forward(r, w, tbl) < VL)
        r = move_index(r, wave_len, tbl);

    return r;
}

template <typename ClockT, typename AudioStreamT>
float AudioProcess<ClockT, AudioStreamT>::reverse_dot_prod(const float* a, const float* b, int a_offset, int a_size, int b_size) {
    float dot = 0.f;
    for (int t = 0; t < b_size; ++t) {
        dot += a[(t + a_offset) % a_size] * b[b_size - 1 - t];
    }
    return dot;
}

// TODO try fft based cross correlation for perf reasons.
template <typename ClockT, typename AudioStreamT>
int AudioProcess<ClockT, AudioStreamT>::cross_correlation_sync(
    const int w, const int r, const int dist, float* history_buff[HISTORY_NUM_FRAMES], const int frame_id, const float* buff) {
    // look through a range of dist samples centered at r
    const int r_begin = move_index(r, -dist / 2, TBL);

    struct result {
        int r;
        float dot;
    };
    std::vector<result> dots(dist / HISTORY_SEARCH_GRANULARITY);
    std::vector<int> indices(dist / HISTORY_SEARCH_GRANULARITY);
    std::iota(indices.begin(), indices.end(), 0);

    // Find r that gives best similarity between buff and history_buff
#ifdef HAVE_PAR_ALGS
    std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
#else
    std::for_each(indices.begin(), indices.end(),
#endif
        [&](const int i) {
        const int local_r = (r_begin + i * HISTORY_SEARCH_GRANULARITY) % TBL;
        float dot = 0.f;
        for (int b = 0; b < HISTORY_NUM_FRAMES; ++b) {
            int cur_buf = (frame_id + b) % HISTORY_NUM_FRAMES;
            dot += reverse_dot_prod(buff, history_buff[cur_buf], local_r, TBL, HISTORY_BUFF_SIZE);
        }
        dots[i] = { local_r, dot };
    });

    std::sort(dots.begin(), dots.end(), [](const result& x, const result& y) { return x.dot > y.dot; });
    return dots[0].r;
}

template <typename ClockT, typename AudioStreamT>
typename ClockT::duration AudioProcess<ClockT, AudioStreamT>::dura(float x) {
    // dura() converts a second, represented as a double, into the appropriate unit of time for
    // ClockT and with the appropriate arithematic type using dura() avoids errors like this :
    // chrono::seconds(double initializer) dura() : <double,seconds> ->
    // chrono::steady_clock::<typeof count(), time unit>
    return chrono::duration_cast<typename ClockT::duration>(
        chrono::duration<float, std::ratio<1>>(x));
}
