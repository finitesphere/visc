#include "fft.h"

#include "audio.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define kiss_fft_scalar float
#include "kiss_fftr.h"

static kiss_fftr_cfg g_fft_cfg = NULL;
static kiss_fft_scalar g_time[VISC_FFT_SIZE];
static kiss_fft_cpx g_freq[VISC_FFT_SIZE / 2 + 1];
static float g_prev_bars[VISC_MAX_BARS];
static float g_prev_peaks[VISC_MAX_BARS];

void vis_analysis_init(void) {
  g_fft_cfg = kiss_fftr_alloc(VISC_FFT_SIZE, 0, NULL, NULL);
  memset(g_prev_bars, 0, sizeof(g_prev_bars));
  memset(g_prev_peaks, 0, sizeof(g_prev_peaks));
}

void vis_analysis_shutdown(void) {
  if (g_fft_cfg) {
    kiss_fftr_free(g_fft_cfg);
    g_fft_cfg = NULL;
  }
}

static float clamp01(float v) {
  if (v < 0.0f) {
    return 0.0f;
  }
  if (v > 1.0f) {
    return 1.0f;
  }
  return v;
}

static void smooth_value(float *state, float target, float attack, float decay, float dt) {
  float rate = target > *state ? attack : decay;
  float alpha = 1.0f - expf(-rate * dt * 60.0f);
  *state += (target - *state) * alpha;
}

static int freq_to_bin(float hz) {
  int bin = (int)(hz * (float)VISC_FFT_SIZE / (float)VISC_SAMPLE_RATE);
  if (bin < 1) {
    bin = 1;
  }
  if (bin > VISC_FFT_SIZE / 2 - 1) {
    bin = VISC_FFT_SIZE / 2 - 1;
  }
  return bin;
}

static float bin_energy(int bin_start, int bin_end) {
  float sum = 0.0f;
  for (int b = bin_start; b <= bin_end; b++) {
    float re = (float)g_freq[b].r;
    float im = (float)g_freq[b].i;
    sum += re * re + im * im;
  }
  int count = bin_end - bin_start + 1;
  return sqrtf(sum / (float)count);
}

void vis_analysis_update(VisAnalysis *out, const float *samples, int sample_count,
                         const VisConfig *cfg, float dt) {
  int bars = cfg->bar_count;
  if (bars < 8) {
    bars = 8;
  }
  if (bars > VISC_MAX_BARS) {
    bars = VISC_MAX_BARS;
  }

  int n = sample_count < VISC_FFT_SIZE ? sample_count : VISC_FFT_SIZE;
  for (int i = 0; i < n; i++) {
    float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(VISC_FFT_SIZE - 1)));
    g_time[i] = samples[i] * w;
    out->waveform[i] = samples[i];
  }
  for (int i = n; i < VISC_FFT_SIZE; i++) {
    g_time[i] = 0.0f;
    out->waveform[i] = 0.0f;
  }

  kiss_fftr(g_fft_cfg, g_time, g_freq);

  float low = cfg->freq_low_hz;
  float high = cfg->freq_high_hz;
  if (high <= low + 10.0f) {
    high = low + 10.0f;
  }

  for (int i = 0; i < bars; i++) {
    float t0 = (float)i / (float)bars;
    float t1 = (float)(i + 1) / (float)bars;
    float f0 = low * powf(high / low, t0);
    float f1 = low * powf(high / low, t1);
    int b0 = freq_to_bin(f0);
    int b1 = freq_to_bin(f1);
    if (b1 < b0) {
      b1 = b0;
    }
    float e = bin_energy(b0, b1);
    float norm = e * cfg->sensitivity * 0.08f;
    norm = clamp01(norm);
    if (norm < cfg->min_bar_norm) {
      norm = cfg->min_bar_norm * 0.25f;
    }
    smooth_value(&g_prev_bars[i], norm, cfg->smooth_attack, cfg->smooth_decay, dt);
    out->bars[i] = g_prev_bars[i];

    if (cfg->show_peaks) {
      if (out->bars[i] > g_prev_peaks[i]) {
        g_prev_peaks[i] = out->bars[i];
      } else {
        g_prev_peaks[i] -= cfg->smooth_decay * dt * 0.35f;
        if (g_prev_peaks[i] < out->bars[i]) {
          g_prev_peaks[i] = out->bars[i];
        }
      }
      out->peaks[i] = g_prev_peaks[i];
    } else {
      out->peaks[i] = 0.0f;
    }
  }

  if (cfg->mode == VIS_MODE_WAVEFORM) {
    int step = VISC_FFT_SIZE / bars;
    if (step < 1) {
      step = 1;
    }
    for (int i = 0; i < bars; i++) {
      float peak = 0.0f;
      int start = i * step;
      int end = start + step;
      if (end > n) {
        end = n;
      }
      for (int s = start; s < end; s++) {
        float a = fabsf(out->waveform[s]);
        if (a > peak) {
          peak = a;
        }
      }
      float norm = clamp01(peak * cfg->sensitivity * 2.5f);
      smooth_value(&g_prev_bars[i], norm, cfg->smooth_attack, cfg->smooth_decay, dt);
      out->bars[i] = g_prev_bars[i];
      out->peaks[i] = cfg->show_peaks ? out->bars[i] : 0.0f;
    }
  }
}

void vis_analysis_peak_info(const VisAnalysis *out, const VisConfig *cfg, float *peak_hz,
                            float *peak_db) {
  int bars = cfg->bar_count;
  if (bars < 8) {
    bars = 8;
  }
  if (bars > VISC_MAX_BARS) {
    bars = VISC_MAX_BARS;
  }

  int peak_i = 0;
  float peak_v = 0.0f;
  for (int i = 0; i < bars; i++) {
    if (out->bars[i] > peak_v) {
      peak_v = out->bars[i];
      peak_i = i;
    }
  }

  float low = cfg->freq_low_hz;
  float high = cfg->freq_high_hz;
  if (high <= low + 10.0f) {
    high = low + 10.0f;
  }
  float t0 = (float)peak_i / (float)bars;
  float t1 = (float)(peak_i + 1) / (float)bars;
  float f0 = low * powf(high / low, t0);
  float f1 = low * powf(high / low, t1);
  if (peak_hz) {
    *peak_hz = 0.5f * (f0 + f1);
  }
  if (peak_db) {
    float amp = fmaxf(peak_v, 1e-6f);
    *peak_db = 20.0f * log10f(amp);
  }
}
