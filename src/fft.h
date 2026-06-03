#ifndef VISC_FFT_H
#define VISC_FFT_H

#include "audio.h"
#include "config.h"

#define VISC_MAX_BARS 256

typedef struct {
  float bars[VISC_MAX_BARS];
  float peaks[VISC_MAX_BARS];
  float waveform[VISC_FFT_SIZE];
} VisAnalysis;

void vis_analysis_init(void);
void vis_analysis_shutdown(void);
void vis_analysis_update(VisAnalysis *out, const float *samples, int sample_count,
                         const VisConfig *cfg, float dt);
void vis_analysis_peak_info(const VisAnalysis *out, const VisConfig *cfg, float *peak_hz,
                            float *peak_db);

#endif
