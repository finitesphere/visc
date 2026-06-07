#include "app.h"

#include "audio.h"
#include "config.h"
#include "fft.h"
#include "stats.h"
#include "themes.h"
#include "ui.h"
#include "visualizer.h"
#include "window.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct VisApp {
  SDL_Window *window;
  SDL_Renderer *renderer;
  VisConfig config;
  VisAnalysis analysis;
  float samples[VISC_FFT_SIZE];
  char mp3_path[512];
  char config_path[512];
  bool show_panel;
  Uint64 last_ticks;
  float anim_time;
  float party_timer;
  bool applied_transparent;
  bool applied_on_top;
};

VisApp *vis_app_create(void) { return (VisApp *)calloc(1, sizeof(VisApp)); }

void vis_app_destroy(VisApp *app) {
  if (!app) {
    return;
  }
  free(app);
}

static void sync_window_modes(VisApp *app) {
  if (!app->window) {
    return;
  }
  if (app->config.always_on_top != app->applied_on_top) {
    vis_window_apply_always_on_top(app->window, app->config.always_on_top);
    app->applied_on_top = app->config.always_on_top;
  }
  if (app->config.transparent != app->applied_transparent) {
    vis_window_apply_transparency(app->window, app->config.transparent);
    app->applied_transparent = app->config.transparent;
  }
  vis_window_apply_drag_anywhere(app->window, !app->config.transparent && !app->show_panel);
}

bool vis_app_init(VisApp *app) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  vis_stats_init();
  vis_config_defaults(&app->config);
  snprintf(app->config_path, sizeof(app->config_path), "%s", "visc.ini");
  {
    int theme = VIS_THEME_VU_METER;
    bool loaded_config = false;
    char remembered[512];
    if (vis_config_load_remembered_path(remembered, sizeof(remembered)) &&
        vis_config_load(&app->config, remembered, &theme)) {
      snprintf(app->config_path, sizeof(app->config_path), "%s", remembered);
      loaded_config = true;
    } else {
      loaded_config = vis_config_load(&app->config, app->config_path, &theme);
    }
    if (!vis_window_create(&app->window, &app->renderer, &app->config)) {
      return false;
    }
    if (!loaded_config) {
      vis_window_set_display_refresh_cap(app->window, &app->config);
    }
    app->applied_transparent = app->config.transparent;
    app->applied_on_top = app->config.always_on_top;
    if (!vis_ui_init(app->window, app->renderer)) {
      fprintf(stderr, "UI init failed\n");
      return false;
    }
    vis_ui_set_theme(theme);
  }

  if (!vis_audio_init()) {
    fprintf(stderr, "PortAudio init failed\n");
    return false;
  }
  vis_analysis_init();

  app->show_panel = true;
  app->last_ticks = SDL_GetPerformanceCounter();
  app->anim_time = 0.0f;
  app->party_timer = 0.0f;
  return true;
}

static void handle_hotkeys(VisApp *app, const SDL_Event *ev) {
  if (ev->type != SDL_KEYDOWN || ev->key.repeat) {
    return;
  }
  switch (ev->key.keysym.sym) {
  case SDLK_TAB:
    app->show_panel = !app->show_panel;
    return;
  case SDLK_f:
    vis_window_toggle_fullscreen_or_minimize(app->window);
    return;
  default:
    break;
  }
  if (vis_ui_want_capture_keyboard()) {
    return;
  }
  switch (ev->key.keysym.sym) {
  case SDLK_ESCAPE:
    SDL_Event quit;
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
    break;
  case SDLK_1:
    app->config.layout = VIS_LAYOUT_VERTICAL;
    break;
  case SDLK_2:
    app->config.layout = VIS_LAYOUT_HORIZONTAL;
    break;
  case SDLK_3:
    app->config.layout = VIS_LAYOUT_CIRCULAR;
    break;
  case SDLK_m:
    app->config.mode =
        app->config.mode == VIS_MODE_SPECTRUM ? VIS_MODE_WAVEFORM : VIS_MODE_SPECTRUM;
    break;
  case SDLK_s:
    app->config.show_stats = !app->config.show_stats;
    break;
  case SDLK_LEFTBRACKET:
    vis_ui_theme_prev(&app->config);
    break;
  case SDLK_RIGHTBRACKET:
    vis_ui_theme_next(&app->config);
    break;
  case SDLK_t:
    app->config.transparent = !app->config.transparent;
    sync_window_modes(app);
    break;
  case SDLK_o:
    app->config.always_on_top = !app->config.always_on_top;
    sync_window_modes(app);
    break;
  case SDLK_p:
    app->config.party_mode = !app->config.party_mode;
    app->party_timer = 0.0f;
    break;
  case SDLK_i:
    app->config.show_stats = !app->config.show_stats;
    break;
  case SDLK_g: {
    int next = ((int)app->config.bg_anim + 1) % VIS_BG_COUNT;
    app->config.bg_anim = (VisBgAnim)next;
    break;
  }
  default:
    break;
  }
}

void vis_app_run(VisApp *app) {
  bool running = true;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      vis_ui_process_event(&ev);
      if (ev.type == SDL_QUIT) {
        running = false;
      } else if (ev.type == SDL_WINDOWEVENT &&
                 (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                  ev.window.event == SDL_WINDOWEVENT_RESIZED)) {
        SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
        SDL_RenderClear(app->renderer);
        SDL_RenderPresent(app->renderer);
      } else if (ev.type == SDL_KEYDOWN) {
        handle_hotkeys(app, &ev);
      }
    }

    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - app->last_ticks) / (float)SDL_GetPerformanceFrequency();
    app->last_ticks = now;
    if (dt > 0.1f) {
      dt = 0.1f;
    }
    app->anim_time += dt;
    vis_stats_on_frame(dt);

    if (app->config.party_mode) {
      app->party_timer += dt;
      if (app->party_timer >= 7.0f) {
        vis_ui_theme_next(&app->config);
        app->party_timer = 0.0f;
      }
    }

    float frame_delay = app->config.fps_cap > 1.0f ? (1.0f / app->config.fps_cap) : 0.0f;
    Uint32 frame_start = SDL_GetTicks();

    vis_audio_read_samples(app->samples, VISC_FFT_SIZE);

    Uint64 fft_t0 = SDL_GetPerformanceCounter();
    vis_analysis_update(&app->analysis, app->samples, VISC_FFT_SIZE, &app->config, dt);
    Uint64 fft_t1 = SDL_GetPerformanceCounter();
    float fft_ms =
        (float)(fft_t1 - fft_t0) * 1000.0f / (float)SDL_GetPerformanceFrequency();
    vis_stats_on_fft(fft_ms);

    float peak_hz = 0.0f;
    float peak_db = -80.0f;
    vis_analysis_peak_info(&app->analysis, &app->config, &peak_hz, &peak_db);
    vis_stats_set_spectrum(peak_hz, peak_db);

    float sample_rate = vis_audio_sample_rate();
    float buffer_ms = (float)vis_audio_available_samples() / sample_rate * 1000.0f;
    float block_ms = (float)VISC_FFT_SIZE / sample_rate * 1000.0f;
    vis_stats_set_audio(sample_rate, buffer_ms + block_ms * 0.5f);

    int w = 0, h = 0;
    SDL_GetRendererOutputSize(app->renderer, &w, &h);

    int bars_drawn =
        vis_draw(app->renderer, w, h, &app->config, &app->analysis, app->anim_time, dt);
    vis_stats_set_bars(app->config.bar_count, bars_drawn);

    vis_ui_begin_frame();
    vis_ui_draw(&app->config, app->window, app->mp3_path, sizeof(app->mp3_path),
                app->config_path, sizeof(app->config_path), &app->show_panel);
    sync_window_modes(app);
    vis_ui_draw_stats_overlay(app->config.show_stats);
    vis_ui_render(app->renderer);

    SDL_RenderPresent(app->renderer);

    if (frame_delay > 0.0f) {
      Uint32 elapsed = SDL_GetTicks() - frame_start;
      Uint32 target_ms = (Uint32)(frame_delay * 1000.0f);
      if (elapsed < target_ms) {
        SDL_Delay(target_ms - elapsed);
      }
    }
  }

  vis_ui_shutdown();
  vis_analysis_shutdown();
  vis_audio_shutdown();
  vis_window_destroy(app->window, app->renderer);
  app->window = NULL;
  app->renderer = NULL;
  SDL_Quit();
}
