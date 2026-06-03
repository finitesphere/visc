#include "window.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static Uint32 window_flags(const VisConfig *cfg) {
  Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
  if (cfg->always_on_top) {
    flags |= SDL_WINDOW_ALWAYS_ON_TOP;
  }
  return flags;
}

static void setup_hints(void) {
#ifdef _WIN32
  SDL_SetHint("SDL_WINDOWS_FORCE_DWM_VIDEOWINDOW", "1");
#endif
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
}

static void setup_renderer(SDL_Renderer *renderer) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void vis_window_apply_transparency(SDL_Window *window, bool enabled) {
  if (!window) {
    return;
  }
#ifdef _WIN32
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info)) {
    fprintf(stderr, "SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
    return;
  }
  HWND hwnd = info.info.win.window;
  if (!hwnd) {
    return;
  }

  LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  if (enabled) {
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    /* Pure black pixels become see-through (matches visualizer clear color). */
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
  } else {
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
  }
#else
  if (enabled) {
    SDL_SetWindowOpacity(window, 0.92f);
  } else {
    SDL_SetWindowOpacity(window, 1.0f);
  }
#endif
}

bool vis_window_create(SDL_Window **window, SDL_Renderer **renderer, const VisConfig *cfg) {
  setup_hints();

  *window = SDL_CreateWindow("visc", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
                             window_flags(cfg));
  if (!*window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  *renderer =
      SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!*renderer) {
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!*renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(*window);
    *window = NULL;
    return false;
  }

  setup_renderer(*renderer);
  vis_window_apply_always_on_top(*window, cfg->always_on_top);
  vis_window_apply_transparency(*window, cfg->transparent);
  return true;
}

void vis_window_destroy(SDL_Window *window, SDL_Renderer *renderer) {
  if (renderer) {
    SDL_DestroyRenderer(renderer);
  }
  if (window) {
    SDL_DestroyWindow(window);
  }
}

void vis_window_apply_always_on_top(SDL_Window *window, bool enabled) {
  if (!window) {
    return;
  }
  SDL_SetWindowAlwaysOnTop(window, enabled ? SDL_TRUE : SDL_FALSE);
}
