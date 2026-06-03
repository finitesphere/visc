#ifndef VISC_AUDIO_LOOPBACK_H
#define VISC_AUDIO_LOOPBACK_H

#include "audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

int vis_audio_refresh_loopback_devices(VisAudioDevice *out, int max_count);
bool vis_loopback_device_name(int loopback_index, char *out, size_t out_size);
bool vis_loopback_start(int loopback_index);
void vis_audio_stop_loopback(void);

#ifdef __cplusplus
}
#endif

#endif
