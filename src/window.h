#ifndef VISC_WINDOW_H
#define VISC_WINDOW_H

#include "config.h"

struct SDL_Window;
struct SDL_Renderer;

#ifdef __cplusplus
extern "C" {
#endif

bool vis_window_create(struct SDL_Window **window, struct SDL_Renderer **renderer,
                       const VisConfig *cfg);
void vis_window_destroy(struct SDL_Window *window, struct SDL_Renderer *renderer);

void vis_window_apply_always_on_top(struct SDL_Window *window, bool enabled);
void vis_window_apply_transparency(struct SDL_Window *window, bool enabled);
void vis_window_apply_drag_anywhere(struct SDL_Window *window, bool enabled);
void vis_window_set_display_refresh_cap(struct SDL_Window *window, VisConfig *cfg);
void vis_window_toggle_fullscreen_or_minimize(struct SDL_Window *window);
void vis_window_minimize(struct SDL_Window *window);
void vis_window_restore_default(struct SDL_Window *window);

#ifdef __cplusplus
}
#endif

#endif
