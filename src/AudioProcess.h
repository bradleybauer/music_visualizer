#pragma once
// vim: foldmarker=\/\-,\-\/:foldmethod=marker

//- Includes
#include <iostream>
using std::cout;
using std::endl;
#include <chrono>
namespace chrono = std::chrono;
#include <cmath>   // std::abs
#include <cstdlib> // malloc and free
#include <limits>  // numeric_limits<T>::infinity()
#include <mutex>

#include <complex>
using std::complex;

#include "AudioStreams/AudioStream.h"
#include "ShaderConfig.h" // AudioOptions

#include "ffts.h"
// -/

static const int VISUALIZER_BUFSIZE = 1024;
struct AudioData {
    float* audio_l;
    float* audio_r;
    float* freq_l;
    float* freq_r;
    bool thread_join;
    std::mutex mtx;
};

// FFT computation library needs aligned memory
// TODO std::aligned_alloc is in c++17 apparently
#ifdef WINDOWS
#define Aligned_Alloc(align, sz) _aligned_malloc(sz, align)
#define Aligned_Free(ptr) _aligned_free(ptr)
#else
#define Aligned_Alloc(align, sz) aligned_alloc(align, sz)
#define Aligned_Free(ptr) free(ptr)
#endif

//- Constants
//
// length of the system audio capture buffer (in frames)
static const int ABL = 512;
// number of system audio capture buffers to keep
static const int ABN = 16;
// total audio buffer length. length of all ABN buffers combined (in frames)
static const int TBL = ABL * ABN;
static const int FFTLEN = TBL / 2;
// length of visualizer 1D texture buffers. Number of frames of audio this module outputs.
static const int VL = VISUALIZER_BUFSIZE;
// channel count of audio
static const int C = 2;
// sample rate of the audio stream
static const int SR = 48000;
// sample rate of audio given to FFT
static const int SRF = SR / 2;

// I think the FFT looks best when it has 8192/48000, or 4096/24000, time granularity. However, I
// like the wave to be 96000hz just because then it fits on the screen nice. To have both an FFT on
// 24000hz data and to display 96000hz waveform data, I ask pulseaudio to output 48000hz and then
// resample accordingly. I've compared the 4096 sample FFT over 24000hz data to the 8192 sample FFT
// over 48000hz data and they are surprisingly similar. I couldn't tell a difference really. I've
// also compared resampling with ffmpeg to my current naive impl. Hard to notice a difference.
// -/

//- Options
// WAVE RENORMALIZATION
// RENORM_1: divide by max absolute value in buffer.
// RENORM_2: a bouncy renorm, the springy bounce effect is what occurs when we normalize by using
//           max = mix(max_prev, max_current, parameter); max_prev = max. So if there is a pulse of
//           amplitude in the sound, then a louder sound is normalized by a smaller maximum and
//           therefore the amplitude is greater than 1. The amplitude will be inflated for a short
//           period of time until the dynamics of the max mixing catch up and max_prev is
//           approximately equal to max_current.
// RENORM_3: divide by the max observed amplitude during lifetime of AudioProcess.
//
// #define RENORM_1
#define RENORM_2
// #define RENORM_3

// DIFF_SYNC OPTIONS
static const int HISTORY_NUM_FRAMES = 9;
static const int HISTORY_SEARCH_GRANULARITY = 2;
static const int HISTORY_SEARCH_RANGE = 16 * HISTORY_SEARCH_GRANULARITY;
static const int HISTORY_BUFF_SZ = VL;
// Observations:
// If I set these params (frames: 3, gran: 4, range:16*gran), then Slow Motion (by TEB) visualizes
// ok, but there is very noticable drift in the wave during the intro (sound=clean constant tone).
// But if I set frames to 9 then drift is reduced (almost eliminated) throughout the whole song.
// This observation shows that the history frames help establish a correlation in frame placement
// (horizontal shift) between new frames and older frames.
//
// The performance cost for the history_search/difference_sync is
//
// 2 * HISTORY_SEARCH_RANGE * HISTORY_NUM_FRAMES * HISTORY_BUFF_SZ
// ---------------------------------------------------------------
//                 HISTORY_SEARCH_GRANULARITY
//
// -/

//- Class Definition
template <typename ClockT, typename AudioStreamT>
class AudioProcess {
public:
    AudioProcess(AudioStreamT& _audio_stream);
    ~AudioProcess();

    void service_channel(int w,
                         int& r,
                         int& frame_id,
                         float* prev_buff[HISTORY_NUM_FRAMES],
                         float* audio_buff,
                         float* staging_buff,
                         const complex<float>* fft_out,
                         float& freq,
                         float& channel_max,
                         typename ClockT::time_point now,
                         typename ClockT::time_point& next_t);

    void step();
    int start() {
        while (!audio_sink.thread_join)
            step();
        return 0;
    }
    void stop() {
        audio_sink.thread_join = true;
    }
    AudioData& get_audio_data() {
        return audio_sink;
    }
    void set_audio_options(AudioOptions& ao) {
        diff_sync = ao.diff_sync;
        fft_sync = ao.fft_sync;
        wave_smoother = ao.wave_smooth;

        // TODO
        ao.fft_smooth;
    }

private:
    // EXPONENTIAL SMOOTHER
    // A smaller wave_smoother makes the wave more like molasses.
    // A larger wave_smoother makes the wave more snappy.
    // Mixes the old buffer of audio with the new buffer of audio.
    float wave_smoother = .97f;

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
    bool fft_sync = true;

    // Reduces drift in the wave that the fft_sync can't stop.
    bool diff_sync = true;

    // Data members
    typename ClockT::time_point now;
    const std::chrono::nanoseconds _60fpsDura = dura(1.f / 60.f);
    typename ClockT::time_point next_l = now + _60fpsDura;
    typename ClockT::time_point next_r = now + _60fpsDura;
    // Compute the fft only once every 1/60 of a second
    typename ClockT::time_point next_fft = now + _60fpsDura;
    int frame_id_l = 0;
    int frame_id_r = 0;

    float* prev_buff_l[HISTORY_NUM_FRAMES];
    float* prev_buff_r[HISTORY_NUM_FRAMES];

    float* audio_buff_l;
    float* audio_buff_r;
    float* staging_buff_l;
    float* staging_buff_r;
    ffts_plan_t* fft_plan;

    complex<float>* fft_outl;
    float* fft_inl;
    complex<float>* fft_outr;
    float* fft_inr;
    float* FFTwindow;

    int writer;
    int reader_l;
    int reader_r;
    float freql;
    float freqr;

    float channel_max_l;
    float channel_max_r;

    float max_amplitude_so_far = 0.;

    struct AudioData audio_sink;
    AudioStreamT& audio_stream;

    // Returns the bin holding the max frequency of the fft. We only consider the first 100 bins.
    static int max_bin(const complex<float>* f);
    static float max_frequency(const complex<float>* f);

    // returns a fraction of freq such that freq / c <= thres for c = some power of two
    static float get_harmonic_less_than(float freq, float thres);

    // Move p around the circular buffer by the amound and direction delta
    // tbl: total buffer length of the circular buffer
    static int move_index(int p, int delta, int tbl);
    static int dist_forward(int from, int to, int tbl);
    static int dist_backward(int from, int to, int tbl);
    static int sign(int x);

    // Attempts to separate two points, in a circular buffer, r (reader) and w (writer)
    // by distance tbl/2.f by moving r in steps of size step_size.
    // Returns the delta that r should be moved by
    static int adjust_reader(int r, int w, int step_size, int tbl);

    static inline float mix(float x, float y, float m);

    // setting low mixer can lead to some interesting lissajous
    static void renorm(float* buff, float& prev_max_amp, float mixer, float scale, int buff_size);
    static void renorm(float* buff, float scale, int buff_size);

    // Adds the wavelength computed by sample_rate / frequency to r.
    // If w is within VL units in front of r, then adjust r so that this is no longer true.
    // w: index of the writer in the circular buffer
    // r: index of the reader in the circular buffer
    // freq: frequency to compute the wavelength with
    static int advance_index(int w, int r, float freq, int tbl);

    // Compute the manhattan distance between two vectors.
    // a_circular: a circular buffer containing the first vector
    // b_buff: a buffer containing the second vector
    // a_sz: circumference of a_circular
    // b_sz: length of b_buff
    static float
    manhattan_dist(const float* a_circular, const float* b_buff, int a_offset, int a_sz, int b_sz);

    // w: writer
    // r: reader
    // dist: half the amount to search by
    // buff: buffer of audio to compare prev_buffer with
    //
    // buffer sizes: Sure, I could make it all variable, but does that mean the function can work
    // with arbitrary inputs? No, the inputs depend on each other too much. Writing the
    // tests/asserts in anticipation that the inputs would vary is too much work, when I know that
    // the inputs actually will not vary.
    static int difference_sync(int w,
                               int r,
                               int dist,
                               float* prev_buff[HISTORY_NUM_FRAMES],
                               int frame_id,
                               const float* buff);

    // converts number of seconds x to a time duration for the ClockT type
    static typename ClockT::duration dura(float x);
};
// -/

//- Constructor
template <typename ClockT, typename AudioStreamT>
AudioProcess<ClockT, AudioStreamT>::AudioProcess(AudioStreamT& _audio_stream)
    : audio_stream(_audio_stream), audio_sink() {
    if (audio_stream.get_sample_rate() != 48000) {
        cout << "The AudioProcess is meant to consume 48000hz audio but the given AudioStream "
             << "produces " << audio_stream.get_sample_rate() << "hz audio." << endl;
        exit(1);
    }
    if (audio_stream.get_max_buff_size() < ABL) {
        cout << "AudioProcess needs at least " << ABL << " frames per call to get_next_pcm"
             << " but the given AudioStream only provides " << audio_stream.get_max_buff_size()
             << "." << endl;
        exit(1);
    }

    //- Internal audio buffers
    audio_buff_l = (float*)calloc(TBL, sizeof(float));
    audio_buff_r = (float*)calloc(TBL, sizeof(float));
    // -/

    //- History buffers
    if (diff_sync)
        for (int i = 0; i < HISTORY_NUM_FRAMES; ++i) {
            prev_buff_l[i] = (float*)calloc(HISTORY_BUFF_SZ, sizeof(float));
            prev_buff_r[i] = (float*)calloc(HISTORY_BUFF_SZ, sizeof(float));
        }
    // -/

    //- Output buffers
    audio_sink.audio_l = (float*)calloc(VL, sizeof(float));
    audio_sink.audio_r = (float*)calloc(VL, sizeof(float));
    audio_sink.freq_l = (float*)calloc(VL, sizeof(float));
    audio_sink.freq_r = (float*)calloc(VL, sizeof(float));
    // -/

    //- Smoothing buffers
    // Helps smooth the appearance of the waveform. Useful when we're not updating the waveform data
    // at 60fps. Holds the audio that is just about ready to be sent to the visualizer. Audio in
    // these buffers are 'staged' to be visualized The staging buffers are mixed with the final
    // output buffers 'continuously' until the staging buffers are updated again. The mixing
    // parameter (wave_smoother) controls a molasses like effect.
    staging_buff_l = (float*)calloc(VL + 1, sizeof(float));
    staging_buff_r = (float*)calloc(VL + 1, sizeof(float));
    // -/

    //- FFT setup
    const int N = FFTLEN;
    fft_plan = ffts_init_1d_real(N, FFTS_FORWARD);
    fft_inl = (float*)Aligned_Alloc(32, N * sizeof(float));
    fft_inr = (float*)Aligned_Alloc(32, N * sizeof(float));
    fft_outl = (complex<float>*)Aligned_Alloc(32, (N / 2 + 1) * sizeof(complex<float>));
    fft_outr = (complex<float>*)Aligned_Alloc(32, (N / 2 + 1) * sizeof(complex<float>));
    FFTwindow = (float*)malloc(N * sizeof(float));
    for (int i = 0; i < N; ++i)
        FFTwindow[i] = (1.f - cosf(2.f * 3.141592f * i / float(N))) / 2.f; // sin(x)^2
    // -/

    //- Buffer Indice Management
    // The index of the writer in the audio buffers, of the newest sample in the buffers, and of a
    // discontinuity in the waveform
    writer = 0;
    // Index of a reader in the audio buffers
    reader_l = TBL - VL;
    reader_r = TBL - VL;
    // Holds a harmonic frequency of the dominant frequency of the audio.
    freql = 60.f;
    // we capture an image of the waveform at this rate
    freqr = freql;
    // -/

    //- Wave Renormalizer
    channel_max_l = 1.f;
    channel_max_r = 1.f;
    // -/
}
// -/

//- Destructor
template <typename ClockT, typename AudioStreamT>
inline AudioProcess<ClockT, AudioStreamT>::~AudioProcess() {
    audio_sink.thread_join = true;
    free(audio_sink.audio_l);
    free(audio_sink.audio_r);
    free(audio_sink.freq_l);
    free(audio_sink.freq_r);
    free(audio_buff_l);
    free(audio_buff_r);
    free(staging_buff_l);
    free(staging_buff_r);

    Aligned_Free(fft_inl);
    Aligned_Free(fft_inr);
    Aligned_Free(fft_outl);
    Aligned_Free(fft_outr);
    free(FFTwindow);

    for (int i = 0; i < HISTORY_NUM_FRAMES; ++i) {
        free(prev_buff_l[i]);
        free(prev_buff_r[i]);
    }

    ffts_free(fft_plan);
}
// -/

template <typename ClockT, typename AudioStreamT>
inline void AudioProcess<ClockT, AudioStreamT>::service_channel(int w,
                                                               int& r,
                                                               int& frame_id,
                                                               float* prev_buff[HISTORY_NUM_FRAMES],
                                                               float* audio_buff,
                                                               float* staging_buff,
                                                               const complex<float>* fft_out,
                                                               float& freq,
                                                               float& channel_max,
                                                               typename ClockT::time_point now,
                                                               typename ClockT::time_point& next_t) {
    if (now > next_t) {
        r = advance_index(w, r, freq, TBL);

        if (diff_sync) {
            r = difference_sync(w, r, HISTORY_SEARCH_RANGE, prev_buff, frame_id, audio_buff);
        }

        for (int i = 0; i < VL; ++i) {
            staging_buff[i] = audio_buff[(i + r) % TBL];

#ifdef RENORM_3
            if (std::abs(staging_buff[i]) > max_amplitude_so_far)
                max_amplitude_so_far = std::abs(staging_buff[i]);
#endif
        }

        // clang-format off
        // Rescale the audio amplitude
        #ifdef RENORM_1
            renorm(staging_buff, channel_max, 1.f, 1.f, VL);
        #endif
        #ifdef RENORM_2
            renorm(staging_buff, channel_max, .3f, .66f, VL);
        #endif
        #ifdef RENORM_3
            renorm(staging_buff, max_amplitude_so_far, 0.f, .83f, VL);
        #endif
        // clang-format on

        // copy the staging buff to the buffer in prev_buff[...]
        if (diff_sync) {
            for (int i = 0; i < HISTORY_BUFF_SZ; ++i) {
                prev_buff[frame_id % HISTORY_NUM_FRAMES][i] = staging_buff[i];
            }
        }
        frame_id++;

        // schedule the next call of service channel
        if (fft_sync) {
            freq = get_harmonic_less_than(max_frequency(fft_out), 80.f);
        }
        else {
            freq = 60.f;
        }
        next_t += dura(1.f / freq);
    }
}

template <typename ClockT, typename AudioStreamT>
inline void AudioProcess<ClockT, AudioStreamT>::step() {
    // TODO On windows this can prevent the app from closing if the music is paused.
    // auto perf_timepoint = ClockT::now();
    audio_stream.get_next_pcm(audio_buff_l + writer, audio_buff_r + writer, ABL);
    // double dt = (ClockT::now() - perf_timepoint).count() / 1e9;
    // summmm += dt;
    // cout << summmm/(frame_id_l +1)<< endl;

    // fps(now);
    now = ClockT::now();
    // We want to use next += dura(1/freq) in the loop below and not next = now + dura(1/freq)
    // because ... TODO, BUT if now >> next, then we probably were stalled in
    // audio_stream.get_next_pcm and should update the next times
    if (now - next_l > chrono::milliseconds(17 * 4)) { // less than 60/2/2 fps
        next_r = now;
        next_l = now;
        next_fft = now;
    }

    //- Manage indices and fill smoothing buffers
    writer = move_index(writer, ABL, TBL);

    service_channel(writer, reader_l, frame_id_l,
                    prev_buff_l,    // previously visualized audio
                    audio_buff_l,   // audio collected from system audio stream
                    staging_buff_l, // audio for visualizer goes here
                    fft_outl,       // fft complex outputs
                    freql,          // most recent dominant frequency < 600hz
                    channel_max_l,  // channel renormalization term
                    now,            // current time point
                    next_l);        // update staging at this timepoint again

    service_channel(writer, reader_r, frame_id_r, prev_buff_r, audio_buff_r, staging_buff_r,
                    fft_outr, freqr, channel_max_r, now, next_r);
    // -/

    //- Execute fft and fill fft output buff
    // This downsamples and windows the audio
    if (now > next_fft) {
        next_fft += _60fpsDura;
        for (int i = 0; i < FFTLEN; ++i) {
#define downsmpl(s) s[(i * 2 + writer) % TBL]
            fft_inl[i] = downsmpl(audio_buff_l) / channel_max_l * FFTwindow[i];
            fft_inr[i] = downsmpl(audio_buff_r) / channel_max_r * FFTwindow[i];
#undef downsmpl
        }
        ffts_execute(fft_plan, fft_inl, fft_outl);
        ffts_execute(fft_plan, fft_inr, fft_outr);
        fft_outl[0] = 0;
        fft_outr[0] = 0;
        audio_sink.mtx.lock();
        // TODO fft magnitudes are different between windows and linux
        for (int i = 0; i < VL; i++) {
            audio_sink.freq_l[i] = std::abs(fft_outl[i]) / std::sqrt(float(FFTLEN));
            audio_sink.freq_r[i] = std::abs(fft_outr[i]) / std::sqrt(float(FFTLEN));
        }
        audio_sink.mtx.unlock();
    }
    // -/

    //- Fill output buffers
    audio_sink.mtx.lock();

    // Smooth and upsample the wave
    for (int i = 0; i < VL; ++i) {
        float oldl, oldr, newl, newr, mixl, mixr;

        newl = staging_buff_l[i];
        newr = staging_buff_r[i];

        oldl = audio_sink.audio_l[i];
        oldr = audio_sink.audio_r[i];

        mixl = mix(oldl, newl, wave_smoother);
        mixr = mix(oldr, newr, wave_smoother);

        //*
        audio_sink.audio_l[i] = mixl;
        audio_sink.audio_r[i] = mixr;
        /*/
        // View the input audio
        //audio_sink.audio_l[i] = audio_buff_l[(i + reader_l)%TBL]/channel_max_l;
        //audio_sink.audio_r[i] = audio_buff_r[(i + reader_r)%TBL]/channel_max_r;
        // View the history frames used in difference_sync
        //audio_sink.audio_l[i] = prev_buff_l[(frame_id_l + HISTORY_NUM_FRAMES -
        1)%HISTORY_NUM_FRAMES][i/2];
        //audio_sink.audio_r[i] = prev_buff_r[(frame_id_r + HISTORY_NUM_FRAMES -
        1)%HISTORY_NUM_FRAMES][i/2];
        // */
    }

    audio_sink.mtx.unlock();
    // -/
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::max_bin(const complex<float>* f) {
    float max = 0.f;
    int max_i = 0;
    // catch frequencies from 5.86 to 586 (that is i * SRF / FFTLEN for i from 1 to 100)
    for (int i = 1; i < 100; ++i) {
        const float mmag = std::abs(f[i]);
        if (mmag > max) {
            max = mmag;
            max_i = i;
        }
    }
    return max_i;
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::move_index(int p, int delta, int tbl) {
    p += delta;
    if (p >= tbl) {
        p -= tbl;
    }
    else if (p < 0) {
        p += tbl;
    }
    return p;
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::dist_forward(int from, int to, int tbl) {
    int d = to - from;
    if (d < 0)
        d += tbl;
    return d;
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::dist_backward(int from, int to, int tbl) {
    return dist_forward(to, from, tbl);
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::sign(int x) {
    if (x < 0)
        return -1;
    else
        return 1;
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::adjust_reader(int r, int w, int step_size, int tbl) {
    const int a = w - r;
    const int b = std::abs(a) - tbl / 2;
    const int foo = std::abs(b);
    // minimize foo(r)
    // foo(r) = |b(a(r))|
    //        = ||w-r|-tbl/2|
    //
    // dfoodr = dfoodb * dbda * dadr
    //
    // dadr   = dadr
    // dbdr   = dbda * dadr
    // dfoodr = dfoodb * dbdr
    //
    // dadr   = -1
    // dbdr   = sign(a) * dadr
    // dfoodr = sign(b) * dbdr
    //
    // -foo'(r) = is the direction r should move to decrease foo(r)
    // foo(r) = happens to be the number of samples such that foo(r - foo'(r)*foo(r)) == 0
    const int dadr = -1;
    const int dbdr = sign(a) * dadr;
    const int dfoodr = sign(b) * dbdr;
    // move r by a magnitude of foo in the -foo'(r) direction, but only take
    // steps of size step_size
    const int grid = int(std::ceil(foo / float(step_size))) * step_size;
    return -dfoodr * grid;
}

template <typename ClockT, typename AudioStreamT>
inline float AudioProcess<ClockT, AudioStreamT>::max_frequency(const complex<float>* f) {
    // more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
    int k = max_bin(f);
    if (k == 0) {
        k = 1;
    }
    const float y1 = std::abs(f[k - 1]); // std:abs(complex) gives magnitudes
    const float y2 = std::abs(f[k]);
    const float y3 = std::abs(f[k + 1]);
    const float d = (y3 - y1) / (2 * (2 * y2 - y1 - y3) + 0.001);
    const float kp = k + d;
    // dont let anything negative or close to zero through
    return std::max(kp * float(SRF) / float(FFTLEN), 10.f);
}

template <typename ClockT, typename AudioStreamT>
inline float AudioProcess<ClockT, AudioStreamT>::get_harmonic_less_than(float freq, float thres) {
    // while (freq > 121.f)
    //     freq /= 2.f;
    const float a = std::log2f(freq);
    const float b = std::log2f(thres);
    freq *= std::pow(2.f, std::floor(b - a));
    if (!std::isnormal(freq))
        freq = 60.f;
    return freq;
}

template <typename ClockT, typename AudioStreamT>
inline float AudioProcess<ClockT, AudioStreamT>::mix(float x, float y, float m) {
    return x * (1.f - m) + y * m;
}

template <typename ClockT, typename AudioStreamT>
inline void AudioProcess<ClockT, AudioStreamT>::renorm(
    float* buff, float& prev_max_amp, float mixer, float scale, int buff_size) {
    float new_max_amp = -16.f;
    for (int i = 0; i < buff_size; ++i)
        if (std::abs(buff[i]) > new_max_amp)
            new_max_amp = std::abs(buff[i]);
    prev_max_amp = mix(prev_max_amp, new_max_amp, mixer);
    for (int i = 0; i < buff_size; ++i)
        buff[i] /= (prev_max_amp + 0.001f) / scale;
}

template <typename ClockT, typename AudioStreamT>
inline void AudioProcess<ClockT, AudioStreamT>::renorm(float* buff, float scale, int buff_size) {
    for (int i = 0; i < buff_size; ++i)
        buff[i] *= scale;
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::advance_index(int w, int r, float freq, int tbl) {
    int wave_len = int(SR / freq + .5f);
    r = move_index(r, wave_len, tbl);
    // if dist from r to w is < what is read by graphics system
    if (dist_forward(r, w, tbl) < VL) {
        int delta = adjust_reader(r, w, wave_len, tbl);
        r = move_index(r, delta, tbl);
        // cout << "Reader too close to writer, adjusting." << endl;
    }
    return r;
}

template <typename ClockT, typename AudioStreamT>
inline float AudioProcess<ClockT, AudioStreamT>::manhattan_dist(
    const float* a_circular, const float* b_buff, int a_offset, int a_sz, int b_sz) {
    float md = 0.f;
    for (int i = 0; i < b_sz; ++i) {
        const float xi_1 = a_circular[(i + a_offset) % a_sz];
        const float xi_2 = b_buff[i];
        md += std::abs(xi_1 - xi_2);
        // md += std::pow(xi_1-xi_2, 2);
    }
    return md;
}

template <typename ClockT, typename AudioStreamT>
inline int AudioProcess<ClockT, AudioStreamT>::difference_sync(
    int w, int r, int dist, float* prev_buff[HISTORY_NUM_FRAMES], int frame_id, const float* buff) {
    if (dist_backward(r, w, TBL) > dist && dist_forward(r, w, TBL) > dist) {
        int orig_r = r;
        r = move_index(r, -dist, TBL);

        // Find r that gives best similarity between buff and prev_buff
        int min_r = orig_r;
        float min_md = std::numeric_limits<float>::infinity();
        for (int i = 0; i < dist * 2 / HISTORY_SEARCH_GRANULARITY; ++i) {
            float md = 0.f;
            for (int b = 0; b < HISTORY_NUM_FRAMES; ++b) {
                int cur_buf = (frame_id + b) % HISTORY_NUM_FRAMES;
                // md += mix(.5f, 1.f, b/float(HISTORY_NUM_FRAMES)) * manhattan_dist(buff,
                // prev_buff[cur_buf], r, TBL, HISTORY_BUFF_SZ);
                md += manhattan_dist(buff, prev_buff[cur_buf], r, TBL, HISTORY_BUFF_SZ);
            }
            if (md < min_md) {
                min_md = md;
                min_r = r;
            }
            r = move_index(r, HISTORY_SEARCH_GRANULARITY, TBL);
        }

        // Find out how far we've been shifted.
        const int df = dist_forward(orig_r, min_r, TBL);
        const int db = dist_backward(orig_r, min_r, TBL);
        int diff;
        if (df < db)
            diff = df;
        else
            diff = -db;

        // We want the pointer to be somewhere around where advanced_index put it. We ONLY want to
        // make a small nudge left or right in order to increase the similarity between this frame
        // and the previous. We do not want to move the index too far. The histogram of where the
        // index is moved can be viewed by defining DIFFERENCE_SYNC_HISTOGRAM_LOG. The histogram
        // shows that, most of the time the index is put either at the ends or somewhere near the
        // center.
        if (std::abs(diff) < dist - 1)
            r = min_r;
        else
            r = orig_r;

#ifdef DIFFERENCE_SYNC_HISTOGRAM_LOG
        static int counts[2 * HISTORY_SEARCH_RANGE] = {};
        if (diff < dist && diff >= -dist)
            counts[diff + dist] += 1;
        if (frame_id % 1000 == 0) {
            for (int i = 0; i < dist * 2; i += HISTORY_SEARCH_GRANULARITY)
                cout << counts[i] << ",";
            cout << endl;
        }
#endif
    }
    return r;
}

template <typename ClockT, typename AudioStreamT>
inline typename ClockT::duration AudioProcess<ClockT, AudioStreamT>::dura(float x) {
    // dura() converts a second, represented as a double, into the appropriate unit of time for
    // ClockT and with the appropriate arithematic type using dura() avoids errors like this :
    // chrono::seconds(double initializer) dura() : <double,seconds> ->
    // chrono::steady_clock::<typeof count(), time unit>
    return chrono::duration_cast<typename ClockT::duration>(
        chrono::duration<float, std::ratio<1>>(x));
}
