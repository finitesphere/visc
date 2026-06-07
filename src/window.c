#include "window.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#endif

#define VISC_DEFAULT_W 1280
#define VISC_DEFAULT_H 720
#define VISC_DRAG_STRIP_H 38
#define VISC_CONTROL_ZONE_W 112
#define VISC_RESIZE_BORDER 12
#define VISC_MIN_W 220
#define VISC_MIN_H 140

#ifdef _WIN32
static WNDPROC g_prev_wnd_proc = NULL;
static bool g_drag_anywhere = false;

static LRESULT CALLBACK visc_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCALCSIZE && wparam) {
    return 0;
  }

  if (msg == WM_GETMINMAXINFO) {
    MINMAXINFO *mmi = (MINMAXINFO *)lparam;
    mmi->ptMinTrackSize.x = VISC_MIN_W;
    mmi->ptMinTrackSize.y = VISC_MIN_H;
    return 0;
  }

  if (msg == WM_NCHITTEST) {
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int x = GET_X_LPARAM(lparam);
    int y = GET_Y_LPARAM(lparam);
    int w = rc.right - rc.left;

    bool left = x >= rc.left && x < rc.left + VISC_RESIZE_BORDER;
    bool right = x <= rc.right && x > rc.right - VISC_RESIZE_BORDER;
    bool top = y >= rc.top && y < rc.top + VISC_RESIZE_BORDER;
    bool bottom = y <= rc.bottom && y > rc.bottom - VISC_RESIZE_BORDER;

    if (top && left) {
      return HTTOPLEFT;
    }
    if (top && right) {
      return HTTOPRIGHT;
    }
    if (bottom && left) {
      return HTBOTTOMLEFT;
    }
    if (bottom && right) {
      return HTBOTTOMRIGHT;
    }
    if (left) {
      return HTLEFT;
    }
    if (right) {
      return HTRIGHT;
    }
    if (top) {
      return HTTOP;
    }
    if (bottom) {
      return HTBOTTOM;
    }

    bool in_controls = x > rc.left + w - VISC_CONTROL_ZONE_W && y < rc.top + VISC_DRAG_STRIP_H;
    if (in_controls) {
      return HTCLIENT;
    }
    if (g_drag_anywhere || y < rc.top + VISC_DRAG_STRIP_H) {
      return HTCAPTION;
    }
    return HTCLIENT;
  }

  return CallWindowProc(g_prev_wnd_proc, hwnd, msg, wparam, lparam);
}
#endif

static Uint32 window_flags(const VisConfig *cfg) {
  Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_BORDERLESS;
  if (cfg->always_on_top) {
    flags |= SDL_WINDOW_ALWAYS_ON_TOP;
  }
  return flags;
}

void vis_window_set_display_refresh_cap(SDL_Window *window, VisConfig *cfg) {
  if (!window || !cfg) {
    return;
  }
  int display_index = SDL_GetWindowDisplayIndex(window);
  if (display_index < 0) {
    display_index = 0;
  }
  int modes = SDL_GetNumDisplayModes(display_index);
  int best_refresh = 0;
  for (int i = 0; i < modes; i++) {
    SDL_DisplayMode mode;
    if (SDL_GetDisplayMode(display_index, i, &mode) == 0 && mode.refresh_rate > best_refresh) {
      best_refresh = mode.refresh_rate;
    }
  }
  if (best_refresh > 0) {
    cfg->fps_cap = (float)best_refresh;
  }
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

#ifdef _WIN32
static HWND window_hwnd(SDL_Window *window) {
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window, &info)) {
    fprintf(stderr, "SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
    return NULL;
  }
  return info.info.win.window;
}

static void apply_borderless_resizable_style(SDL_Window *window) {
  HWND hwnd = window_hwnd(window);
  if (!hwnd) {
    return;
  }
  LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
  style &= ~(LONG_PTR)WS_CAPTION;
  style |= WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  SetWindowLongPtr(hwnd, GWL_STYLE, style);
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void install_window_proc(SDL_Window *window) {
  HWND hwnd = window_hwnd(window);
  if (!hwnd) {
    return;
  }
  if (!g_prev_wnd_proc) {
    g_prev_wnd_proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)visc_wnd_proc);
  }
}
#endif

static SDL_HitTestResult window_hit_test(SDL_Window *window, const SDL_Point *area, void *data) {
  (void)data;
  if (!window || !area) {
    return SDL_HITTEST_NORMAL;
  }
  Uint32 flags = SDL_GetWindowFlags(window);
  if ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) || (flags & SDL_WINDOW_MAXIMIZED)) {
    return SDL_HITTEST_NORMAL;
  }
  int w = 0;
  int h = 0;
  SDL_GetWindowSize(window, &w, &h);

  bool left = area->x >= 0 && area->x < VISC_RESIZE_BORDER;
  bool right = area->x >= w - VISC_RESIZE_BORDER && area->x < w;
  bool top = area->y >= 0 && area->y < VISC_RESIZE_BORDER;
  bool bottom = area->y >= h - VISC_RESIZE_BORDER && area->y < h;

  if (top && left) {
    return SDL_HITTEST_RESIZE_TOPLEFT;
  }
  if (top && right) {
    return SDL_HITTEST_RESIZE_TOPRIGHT;
  }
  if (bottom && left) {
    return SDL_HITTEST_RESIZE_BOTTOMLEFT;
  }
  if (bottom && right) {
    return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
  }
  if (left) {
    return SDL_HITTEST_RESIZE_LEFT;
  }
  if (right) {
    return SDL_HITTEST_RESIZE_RIGHT;
  }
  if (top) {
    return SDL_HITTEST_RESIZE_TOP;
  }
  if (bottom) {
    return SDL_HITTEST_RESIZE_BOTTOM;
  }

  if (area->x > w - VISC_CONTROL_ZONE_W && area->y < VISC_DRAG_STRIP_H) {
    return SDL_HITTEST_NORMAL;
  }
  if (area->y >= 0 && area->y < VISC_DRAG_STRIP_H) {
    return SDL_HITTEST_DRAGGABLE;
  }
  return SDL_HITTEST_NORMAL;
}

void vis_window_apply_transparency(SDL_Window *window, bool enabled) {
  if (!window) {
    return;
  }
#ifdef _WIN32
  HWND hwnd = window_hwnd(window);
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

void vis_window_apply_drag_anywhere(SDL_Window *window, bool enabled) {
  if (!window) {
    return;
  }
#ifdef _WIN32
  g_drag_anywhere = enabled;
#else
  (void)enabled;
#endif
}

bool vis_window_create(SDL_Window **window, SDL_Renderer **renderer, const VisConfig *cfg) {
  setup_hints();

  *window = SDL_CreateWindow("visc", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, VISC_DEFAULT_W,
                             VISC_DEFAULT_H, window_flags(cfg));
  if (!*window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_SetWindowMinimumSize(*window, VISC_MIN_W, VISC_MIN_H);

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
#ifdef _WIN32
  apply_borderless_resizable_style(*window);
#endif
#ifdef _WIN32
  install_window_proc(*window);
#else
  SDL_SetWindowHitTest(*window, window_hit_test, NULL);
#endif
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

void vis_window_toggle_fullscreen_or_minimize(SDL_Window *window) {
  if (!window) {
    return;
  }
  Uint32 flags = SDL_GetWindowFlags(window);
  if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
    SDL_SetWindowFullscreen(window, 0);
    SDL_RestoreWindow(window);
#ifdef _WIN32
    apply_borderless_resizable_style(window);
#endif
    SDL_RaiseWindow(window);
#ifdef _WIN32
    HWND hwnd = window_hwnd(window);
    if (hwnd) {
      SetForegroundWindow(hwnd);
    }
#endif
  } else {
    SDL_RestoreWindow(window);
    SDL_RaiseWindow(window);
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
  }
}

void vis_window_minimize(SDL_Window *window) {
  if (window) {
    SDL_MinimizeWindow(window);
  }
}

void vis_window_restore_default(SDL_Window *window) {
  if (!window) {
    return;
  }
  SDL_SetWindowFullscreen(window, 0);
  SDL_RestoreWindow(window);
#ifdef _WIN32
  apply_borderless_resizable_style(window);
#endif
  SDL_SetWindowSize(window, VISC_DEFAULT_W, VISC_DEFAULT_H);
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_RaiseWindow(window);
}
