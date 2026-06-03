/* Windows WASAPI loopback — captures audio playing on a speaker/output device. */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>

#include "audio_loopback.h"

#include <stdio.h>
#include <string.h>
#include <vector>

#ifndef AUDCLNT_STREAMFLAGS_LOOPBACK
#define AUDCLNT_STREAMFLAGS_LOOPBACK 0x00020000
#endif

struct LoopbackDevice {
  wchar_t id[256];
  char name[256];
  int max_channels;
};

static std::vector<LoopbackDevice> g_loopback_devices;
static IAudioClient *g_audio_client = nullptr;
static IAudioCaptureClient *g_capture_client = nullptr;
static HANDLE g_capture_thread = NULL;
static volatile int g_loopback_stop = 0;
static int g_loopback_channels = 2;
static WAVEFORMATEX *g_mix_format = nullptr;

extern "C" void vis_audio_ring_push(const float *samples, int count);
extern "C" void vis_audio_ring_clear(void);

static bool mix_format_is_float(const WAVEFORMATEX *fmt) {
  if (!fmt) {
    return false;
  }
  if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    return true;
  }
  if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *)fmt;
    return IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != FALSE;
  }
  return false;
}

static bool mix_format_is_pcm16(const WAVEFORMATEX *fmt) {
  if (!fmt) {
    return false;
  }
  if (fmt->wFormatTag == WAVE_FORMAT_PCM && fmt->wBitsPerSample == 16) {
    return true;
  }
  if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *)fmt;
    return IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) != FALSE &&
           ext->Samples.wValidBitsPerSample == 16;
  }
  return false;
}

static void wide_to_utf8(const wchar_t *wide, char *out, size_t out_size) {
  if (!wide || !out || out_size == 0) {
    return;
  }
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)out_size, NULL, NULL);
}

static bool device_name(IMMDevice *device, char *out, size_t out_size) {
  IPropertyStore *props = nullptr;
  PROPVARIANT var;
  PropVariantInit(&var);
  bool ok = false;

  if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) {
    return false;
  }
  if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR) {
    wide_to_utf8(var.pwszVal, out, out_size);
    ok = true;
  }
  PropVariantClear(&var);
  props->Release();
  return ok;
}

int vis_audio_refresh_loopback_devices(VisAudioDevice *out, int max_count) {
  g_loopback_devices.clear();

  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  bool com_inited = SUCCEEDED(hr);
  if (hr == RPC_E_CHANGED_MODE) {
    com_inited = false;
  }

  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDeviceCollection *collection = nullptr;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator), (void **)&enumerator);
  if (FAILED(hr)) {
    if (com_inited) {
      CoUninitialize();
    }
    return 0;
  }

  hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
  if (FAILED(hr)) {
    enumerator->Release();
    if (com_inited) {
      CoUninitialize();
    }
    return 0;
  }

  UINT count = 0;
  collection->GetCount(&count);

  for (UINT i = 0; i < count; i++) {
    IMMDevice *device = nullptr;
    if (FAILED(collection->Item(i, &device))) {
      continue;
    }

    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id))) {
      device->Release();
      continue;
    }

    LoopbackDevice entry = {};
    wcsncpy(entry.id, id, sizeof(entry.id) / sizeof(entry.id[0]) - 1);
    CoTaskMemFree(id);

    if (!device_name(device, entry.name, sizeof(entry.name))) {
      snprintf(entry.name, sizeof(entry.name), "Output %u", i);
    }
    entry.max_channels = 2;
    g_loopback_devices.push_back(entry);
    device->Release();
  }

  collection->Release();
  enumerator->Release();
  if (com_inited) {
    CoUninitialize();
  }

  int written = 0;
  for (size_t i = 0; i < g_loopback_devices.size() && written < max_count; i++) {
    snprintf(out[written].name, sizeof(out[written].name), "[Desktop] %s",
             g_loopback_devices[i].name);
    out[written].index = (int)i;
    out[written].max_input_channels = g_loopback_devices[i].max_channels;
    written++;
  }
  return written;
}

bool vis_loopback_device_name(int loopback_index, char *out, size_t out_size) {
  if (!out || out_size == 0 || loopback_index < 0 ||
      loopback_index >= (int)g_loopback_devices.size()) {
    return false;
  }
  snprintf(out, out_size, "%s", g_loopback_devices[loopback_index].name);
  return true;
}

static DWORD WINAPI capture_thread(LPVOID) {
  while (!g_loopback_stop) {
    if (!g_capture_client) {
      Sleep(10);
      continue;
    }

    UINT32 packet = 0;
    if (FAILED(g_capture_client->GetNextPacketSize(&packet)) || packet == 0) {
      Sleep(5);
      continue;
    }

    while (packet > 0 && !g_loopback_stop) {
      BYTE *data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (FAILED(g_capture_client->GetBuffer(&data, &frames, &flags, NULL, NULL))) {
        break;
      }

      if (data && frames > 0 && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
        const int ch = g_loopback_channels > 0 ? g_loopback_channels : 2;
        float mono[4096];
        UINT32 n = frames;
        if (n > sizeof(mono) / sizeof(mono[0])) {
          n = (UINT32)(sizeof(mono) / sizeof(mono[0]));
        }

        if (mix_format_is_float(g_mix_format)) {
          const float *f = (const float *)data;
          for (UINT32 i = 0; i < n; i++) {
            if (ch >= 2) {
              mono[i] = 0.5f * (f[i * ch] + f[i * ch + 1]);
            } else {
              mono[i] = f[i];
            }
          }
        } else if (mix_format_is_pcm16(g_mix_format)) {
          const int16_t *s = (const int16_t *)data;
          for (UINT32 i = 0; i < n; i++) {
            if (ch >= 2) {
              mono[i] = (1.0f / 32768.0f) * 0.5f * ((float)s[i * ch] + (float)s[i * ch + 1]);
            } else {
              mono[i] = (float)s[i] * (1.0f / 32768.0f);
            }
          }
        }
        vis_audio_ring_push(mono, (int)n);
      }

      g_capture_client->ReleaseBuffer(frames);
      g_capture_client->GetNextPacketSize(&packet);
    }
  }
  return 0;
}

static void close_loopback_stream(void) {
  g_loopback_stop = 1;
  if (g_capture_thread) {
    WaitForSingleObject(g_capture_thread, INFINITE);
    CloseHandle(g_capture_thread);
    g_capture_thread = NULL;
  }
  if (g_capture_client) {
    g_capture_client->Release();
    g_capture_client = nullptr;
  }
  if (g_audio_client) {
    g_audio_client->Stop();
    g_audio_client->Release();
    g_audio_client = nullptr;
  }
  if (g_mix_format) {
    CoTaskMemFree(g_mix_format);
    g_mix_format = nullptr;
  }
}

bool vis_loopback_start(int loopback_index) {
  vis_audio_stop_loopback();
  vis_audio_ring_clear();

  if (loopback_index < 0 || loopback_index >= (int)g_loopback_devices.size()) {
    return false;
  }

  const LoopbackDevice &target = g_loopback_devices[(size_t)loopback_index];

  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    return false;
  }

  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDevice *device = nullptr;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator), (void **)&enumerator);
  if (FAILED(hr)) {
    return false;
  }

  hr = enumerator->GetDevice(target.id, &device);
  enumerator->Release();
  if (FAILED(hr)) {
    return false;
  }

  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&g_audio_client);
  device->Release();
  if (FAILED(hr)) {
    return false;
  }

  hr = g_audio_client->GetMixFormat(&g_mix_format);
  if (FAILED(hr)) {
    close_loopback_stream();
    return false;
  }

  g_loopback_channels = g_mix_format->nChannels;
  if (g_loopback_channels < 1) {
    g_loopback_channels = 2;
  }

  REFERENCE_TIME buffer_duration = 10000000;
  hr = g_audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                  buffer_duration, 0, g_mix_format, NULL);
  if (FAILED(hr)) {
    close_loopback_stream();
    return false;
  }

  hr = g_audio_client->GetService(__uuidof(IAudioCaptureClient), (void **)&g_capture_client);
  if (FAILED(hr)) {
    close_loopback_stream();
    return false;
  }

  hr = g_audio_client->Start();
  if (FAILED(hr)) {
    close_loopback_stream();
    return false;
  }

  g_loopback_stop = 0;
  g_capture_thread = CreateThread(NULL, 0, capture_thread, NULL, 0, NULL);
  if (!g_capture_thread) {
    close_loopback_stream();
    return false;
  }
  return true;
}

void vis_audio_stop_loopback(void) { close_loopback_stream(); }

#else

int vis_audio_refresh_loopback_devices(VisAudioDevice *out, int max_count) {
  (void)out;
  (void)max_count;
  return 0;
}

bool vis_loopback_device_name(int loopback_index, char *out, size_t out_size) {
  (void)loopback_index;
  (void)out;
  (void)out_size;
  return false;
}

bool vis_loopback_start(int loopback_index) {
  (void)loopback_index;
  return false;
}

void vis_audio_stop_loopback(void) {}

#endif
