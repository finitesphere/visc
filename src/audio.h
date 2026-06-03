#ifndef VISC_AUDIO_H
#define VISC_AUDIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

#define VISC_SAMPLE_RATE 44100
#define VISC_FFT_SIZE 2048
#define VISC_RING_SAMPLES (VISC_FFT_SIZE * 8)

typedef enum {
  VISC_AUDIO_DEVICE = 0,
  VISC_AUDIO_LOOPBACK = 1,
  VISC_AUDIO_FILE = 2
} VisAudioSourceKind;

typedef struct {
  int index;
  char name[256];
  int max_input_channels;
} VisAudioDevice;

bool vis_audio_init(void);
void vis_audio_shutdown(void);

int vis_audio_refresh_devices(VisAudioDevice *out, int max_count);
int vis_audio_refresh_loopback_devices(VisAudioDevice *out, int max_count);
bool vis_audio_start_device(int device_index);
bool vis_audio_start_loopback(int loopback_index);
bool vis_audio_start_file(const char *path);
void vis_audio_stop(void);

VisAudioSourceKind vis_audio_source_kind(void);
const char *vis_audio_current_label(void);

bool vis_audio_read_samples(float *mono_out, int count);
int vis_audio_available_samples(void);
float vis_audio_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif
