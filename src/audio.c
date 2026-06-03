#include "audio.h"

#include "audio_loopback.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <portaudio.h>

typedef struct {
  float data[VISC_RING_SAMPLES];
  int write_pos;
  int read_pos;
#ifdef _WIN32
  CRITICAL_SECTION lock;
#else
  pthread_mutex_t lock;
#endif
} SampleRing;

static SampleRing g_ring;
static PaStream *g_stream = NULL;
static VisAudioSourceKind g_kind = VISC_AUDIO_DEVICE;
static char g_label[512] = "Stopped";
static int g_device_index = -1;
static int g_input_channels = 1;

#ifdef _WIN32
static HANDLE g_file_thread = NULL;
static volatile int g_file_stop = 0;
#else
static pthread_t g_file_thread;
static volatile int g_file_stop = 0;
#endif

static void ring_init(void) {
  memset(&g_ring, 0, sizeof(g_ring));
#ifdef _WIN32
  InitializeCriticalSection(&g_ring.lock);
#else
  pthread_mutex_init(&g_ring.lock, NULL);
#endif
}

static void ring_lock(void) {
#ifdef _WIN32
  EnterCriticalSection(&g_ring.lock);
#else
  pthread_mutex_lock(&g_ring.lock);
#endif
}

static void ring_unlock(void) {
#ifdef _WIN32
  LeaveCriticalSection(&g_ring.lock);
#else
  pthread_mutex_unlock(&g_ring.lock);
#endif
}

static void ring_push(const float *samples, int count) {
  ring_lock();
  for (int i = 0; i < count; i++) {
    g_ring.data[g_ring.write_pos % VISC_RING_SAMPLES] = samples[i];
    g_ring.write_pos++;
    if (g_ring.write_pos - g_ring.read_pos > VISC_RING_SAMPLES) {
      g_ring.read_pos = g_ring.write_pos - VISC_RING_SAMPLES;
    }
  }
  ring_unlock();
}

void vis_audio_ring_push(const float *samples, int count) { ring_push(samples, count); }

void vis_audio_ring_clear(void) {
  ring_lock();
  g_ring.read_pos = 0;
  g_ring.write_pos = 0;
  ring_unlock();
}

static int ring_pop(float *out, int count) {
  ring_lock();
  int avail = g_ring.write_pos - g_ring.read_pos;
  if (avail > VISC_RING_SAMPLES) {
    g_ring.read_pos = g_ring.write_pos - VISC_RING_SAMPLES;
    avail = VISC_RING_SAMPLES;
  }
  int n = avail < count ? avail : count;
  for (int i = 0; i < n; i++) {
    out[i] = g_ring.data[(g_ring.read_pos + i) % VISC_RING_SAMPLES];
  }
  g_ring.read_pos += n;
  ring_unlock();
  return n;
}

static int pa_callback(const void *input, void *output, unsigned long frame_count,
                       const PaStreamCallbackTimeInfo *time_info,
                       PaStreamCallbackFlags status_flags, void *user_data) {
  (void)output;
  (void)time_info;
  (void)status_flags;
  (void)user_data;
  if (!input) {
    return paContinue;
  }
  const float *in = (const float *)input;
  float mono[512];
  unsigned long n = frame_count;
  if (n > sizeof(mono) / sizeof(mono[0])) {
    n = sizeof(mono) / sizeof(mono[0]);
  }
  for (unsigned long i = 0; i < n; i++) {
    if (g_input_channels == 2) {
      mono[i] = 0.5f * (in[i * 2] + in[i * 2 + 1]);
    } else {
      mono[i] = in[i];
    }
  }
  ring_push(mono, (int)n);
  return paContinue;
}

bool vis_audio_init(void) {
  ring_init();
  PaError err = Pa_Initialize();
  return err == paNoError;
}

void vis_audio_shutdown(void) {
  vis_audio_stop();
  Pa_Terminate();
#ifdef _WIN32
  DeleteCriticalSection(&g_ring.lock);
#else
  pthread_mutex_destroy(&g_ring.lock);
#endif
}

int vis_audio_refresh_devices(VisAudioDevice *out, int max_count) {
  int n = Pa_GetDeviceCount();
  if (n < 0) {
    return 0;
  }
  int written = 0;
  for (int i = 0; i < n && written < max_count; i++) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
    if (!info || info->maxInputChannels < 1) {
      continue;
    }
    out[written].index = i;
    out[written].max_input_channels = info->maxInputChannels;
    snprintf(out[written].name, sizeof(out[written].name), "%s", info->name);
    written++;
  }
  return written;
}

static void stop_stream(void) {
  if (g_stream) {
    Pa_StopStream(g_stream);
    Pa_CloseStream(g_stream);
    g_stream = NULL;
  }
}

static void stop_file_thread(void) {
  if (g_kind != VISC_AUDIO_FILE) {
    return;
  }
  g_file_stop = 1;
#ifdef _WIN32
  if (g_file_thread) {
    WaitForSingleObject(g_file_thread, INFINITE);
    CloseHandle(g_file_thread);
    g_file_thread = NULL;
  }
#else
  pthread_join(g_file_thread, NULL);
#endif
  g_file_stop = 0;
}

void vis_audio_stop(void) {
  stop_stream();
  stop_file_thread();
  vis_audio_stop_loopback();
  snprintf(g_label, sizeof(g_label), "Stopped");
}

bool vis_audio_start_loopback(int loopback_index) {
  vis_audio_stop();
  if (!vis_loopback_start(loopback_index)) {
    return false;
  }
  g_kind = VISC_AUDIO_LOOPBACK;
  g_device_index = loopback_index;
  char dev_name[256];
  if (vis_loopback_device_name(loopback_index, dev_name, sizeof(dev_name))) {
    snprintf(g_label, sizeof(g_label), "Desktop: %s", dev_name);
  } else {
    snprintf(g_label, sizeof(g_label), "Desktop audio");
  }
  return true;
}

bool vis_audio_start_device(int device_index) {
  vis_audio_stop();
  g_kind = VISC_AUDIO_DEVICE;
  g_device_index = device_index;

  const PaDeviceInfo *info = Pa_GetDeviceInfo(device_index);
  if (!info) {
    return false;
  }

  PaStreamParameters params;
  params.device = device_index;
  g_input_channels = 1;
  params.channelCount = 1;
  if (info->maxInputChannels >= 2) {
    params.channelCount = 2;
    g_input_channels = 2;
  }
  params.sampleFormat = paFloat32;
  params.suggestedLatency = info->defaultLowInputLatency;
  params.hostApiSpecificStreamInfo = NULL;

  PaError err =
      Pa_OpenStream(&g_stream, &params, NULL, VISC_SAMPLE_RATE, paFramesPerBufferUnspecified,
                    paClipOff, pa_callback, NULL);
  if (err != paNoError) {
    return false;
  }
  err = Pa_StartStream(g_stream);
  if (err != paNoError) {
    Pa_CloseStream(g_stream);
    g_stream = NULL;
    return false;
  }
  snprintf(g_label, sizeof(g_label), "Mic: %s", info->name);
  return true;
}

#ifdef _WIN32
static DWORD WINAPI file_thread_proc(LPVOID arg)
#else
static void *file_thread_proc(void *arg)
#endif
{
  const char *path = (const char *)arg;
  FILE *f = fopen(path, "rb");
  if (!f) {
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
  }

  mp3dec_t dec;
  mp3dec_init(&dec);
  mp3dec_frame_info_t info;

  uint8_t file_buf[256 * 1024];
  uint8_t mp3_buf[4096];
  int mp3_len = 0;
  float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

  while (!g_file_stop) {
    if (mp3_len < (int)sizeof(mp3_buf) / 2) {
      size_t rd = fread(mp3_buf + mp3_len, 1, sizeof(mp3_buf) - (size_t)mp3_len, f);
      if (rd == 0) {
        fseek(f, 0, SEEK_SET);
        mp3_len = 0;
        continue;
      }
      mp3_len += (int)rd;
    }

    int samples = mp3dec_decode_frame(&dec, mp3_buf, mp3_len, pcm, &info);
    if (samples > 0 && info.frame_bytes > 0) {
      float mono[MINIMP3_MAX_SAMPLES_PER_FRAME];
      int frames = samples / info.channels;
      for (int i = 0; i < frames; i++) {
        if (info.channels == 1) {
          mono[i] = pcm[i];
        } else {
          mono[i] = 0.5f * (pcm[i * 2] + pcm[i * 2 + 1]);
        }
      }
      ring_push(mono, frames);
    }

    if (info.frame_bytes > 0) {
      memmove(mp3_buf, mp3_buf + info.frame_bytes, (size_t)mp3_len - (size_t)info.frame_bytes);
      mp3_len -= info.frame_bytes;
    } else if (mp3_len > 0) {
      memmove(mp3_buf, mp3_buf + 1, (size_t)mp3_len - 1);
      mp3_len--;
    }

    (void)file_buf;
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec ts = {0, 1000000};
    nanosleep(&ts, NULL);
#endif
  }

  fclose(f);
#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

bool vis_audio_start_file(const char *path) {
  vis_audio_stop();
  g_kind = VISC_AUDIO_FILE;
  g_file_stop = 0;

  static char path_copy[512];
  snprintf(path_copy, sizeof(path_copy), "%s", path);

#ifdef _WIN32
  g_file_thread = CreateThread(NULL, 0, file_thread_proc, path_copy, 0, NULL);
  if (!g_file_thread) {
    return false;
  }
#else
  if (pthread_create(&g_file_thread, NULL, file_thread_proc, path_copy) != 0) {
    return false;
  }
#endif

  const char *base = strrchr(path, '\\');
  if (!base) {
    base = strrchr(path, '/');
  }
  if (base) {
    base++;
  } else {
    base = path;
  }
  snprintf(g_label, sizeof(g_label), "File: %s", base);
  return true;
}

VisAudioSourceKind vis_audio_source_kind(void) { return g_kind; }

const char *vis_audio_current_label(void) { return g_label; }

int vis_audio_available_samples(void) {
  ring_lock();
  int avail = g_ring.write_pos - g_ring.read_pos;
  ring_unlock();
  return avail;
}

float vis_audio_sample_rate(void) { return (float)VISC_SAMPLE_RATE; }

bool vis_audio_read_samples(float *mono_out, int count) {
  int n = ring_pop(mono_out, count);
  if (n < count) {
    memset(mono_out + n, 0, (size_t)(count - n) * sizeof(float));
  }
  return n > 0;
}
