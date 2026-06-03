#include "stats.h"

#include <math.h>
#include <string.h>

static VisStats g_stats;
static float g_fps_accum;
static int g_fps_frames;
static float g_fft_sum;
static int g_fft_count;

void vis_stats_init(void) {
  memset(&g_stats, 0, sizeof(g_stats));
  g_stats.sample_rate_hz = 44100.0f;
  g_stats.fft_ms_min = 1e9f;
  g_stats.fft_ms_max = 0.0f;
  g_fps_accum = 0.0f;
  g_fps_frames = 0;
  g_fft_sum = 0.0f;
  g_fft_count = 0;
}

void vis_stats_on_frame(float frame_dt_sec) {
  if (frame_dt_sec <= 0.0f) {
    return;
  }
  g_fps_accum += frame_dt_sec;
  g_fps_frames++;
  if (g_fps_accum >= 0.4f) {
    g_stats.fps = (float)g_fps_frames / g_fps_accum;
    g_fps_accum = 0.0f;
    g_fps_frames = 0;
  }
}

void vis_stats_on_fft(float fft_ms) {
  if (fft_ms < g_stats.fft_ms_min) {
    g_stats.fft_ms_min = fft_ms;
  }
  if (fft_ms > g_stats.fft_ms_max) {
    g_stats.fft_ms_max = fft_ms;
  }
  g_fft_sum += fft_ms;
  g_fft_count++;
  if (g_fft_count >= 30) {
    g_stats.fft_ms_avg = g_fft_sum / (float)g_fft_count;
    g_fft_sum = 0.0f;
    g_fft_count = 0;
  }
}

void vis_stats_set_audio(float sample_rate_hz, float latency_ms) {
  g_stats.sample_rate_hz = sample_rate_hz;
  g_stats.latency_ms = latency_ms;
}

void vis_stats_set_spectrum(float peak_hz, float peak_db) {
  g_stats.peak_hz = peak_hz;
  g_stats.peak_db = peak_db;
}

void vis_stats_set_bars(int requested, int drawn) {
  g_stats.bars_requested = requested;
  g_stats.bars_drawn = drawn;
}

const VisStats *vis_stats_get(void) { return &g_stats; }
