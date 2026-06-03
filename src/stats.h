#ifndef VISC_STATS_H
#define VISC_STATS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float fps;
  float fft_ms_avg;
  float fft_ms_min;
  float fft_ms_max;
  float peak_hz;
  float peak_db;
  float sample_rate_hz;
  float latency_ms;
  float fft_bin_hz;
  int bars_requested;
  int bars_drawn;
} VisStats;

void vis_stats_init(void);
void vis_stats_on_frame(float frame_dt_sec);
void vis_stats_on_fft(float fft_ms);
void vis_stats_set_audio(float sample_rate_hz, float latency_ms);
void vis_stats_set_spectrum(float peak_hz, float peak_db);
void vis_stats_set_bars(int requested, int drawn);
const VisStats *vis_stats_get(void);

#ifdef __cplusplus
}
#endif

#endif
