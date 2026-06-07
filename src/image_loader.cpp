#include "image_loader.h"

#include <SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>

#include <string>

static ULONG_PTR g_gdiplus_token = 0;

static bool ensure_gdiplus(void) {
  if (g_gdiplus_token) {
    return true;
  }
  Gdiplus::GdiplusStartupInput input;
  return Gdiplus::GdiplusStartup(&g_gdiplus_token, &input, nullptr) == Gdiplus::Ok;
}

static std::wstring widen_path(const char *path) {
  if (!path) {
    return std::wstring();
  }
  int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
  if (len <= 1) {
    len = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
    if (len <= 1) {
      return std::wstring();
    }
    std::wstring out((size_t)len - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, path, -1, &out[0], len);
    return out;
  }
  std::wstring out((size_t)len - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, path, -1, &out[0], len);
  return out;
}

extern "C" SDL_Surface *vis_image_load_surface(const char *path) {
  if (!ensure_gdiplus()) {
    return nullptr;
  }
  std::wstring wide = widen_path(path);
  if (wide.empty()) {
    return nullptr;
  }

  Gdiplus::Bitmap bitmap(wide.c_str());
  if (bitmap.GetLastStatus() != Gdiplus::Ok) {
    return nullptr;
  }

  int width = (int)bitmap.GetWidth();
  int height = (int)bitmap.GetHeight();
  if (width <= 0 || height <= 0) {
    return nullptr;
  }

  Gdiplus::Rect rect(0, 0, width, height);
  Gdiplus::BitmapData data;
  if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) !=
      Gdiplus::Ok) {
    return nullptr;
  }

  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  if (!surface) {
    bitmap.UnlockBits(&data);
    return nullptr;
  }

  Uint8 *src_base = (Uint8 *)data.Scan0;
  Uint8 *dst_base = (Uint8 *)surface->pixels;
  for (int y = 0; y < height; y++) {
    Uint8 *src = src_base + y * data.Stride;
    Uint8 *dst = dst_base + y * surface->pitch;
    for (int x = 0; x < width; x++) {
      dst[x * 4 + 0] = src[x * 4 + 2];
      dst[x * 4 + 1] = src[x * 4 + 1];
      dst[x * 4 + 2] = src[x * 4 + 0];
      dst[x * 4 + 3] = src[x * 4 + 3];
    }
  }

  bitmap.UnlockBits(&data);
  return surface;
}
#else
extern "C" SDL_Surface *vis_image_load_surface(const char *path) {
  return SDL_LoadBMP(path);
}
#endif
