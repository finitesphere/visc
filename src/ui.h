#ifndef VISC_UI_H
#define VISC_UI_H

#include "config.h"

struct SDL_Renderer;
struct SDL_Window;
typedef union SDL_Event SDL_Event;

#ifdef __cplusplus
extern "C" {
#endif

bool vis_ui_init(struct SDL_Window *window, struct SDL_Renderer *renderer);
void vis_ui_shutdown(void);
void vis_ui_begin_frame(void);
void vis_ui_draw(VisConfig *cfg, struct SDL_Window *window, char *file_path, size_t file_path_size,
                 char *config_path, size_t config_path_size, bool *show_panel);
void vis_ui_draw_stats_overlay(bool show);
void vis_ui_set_theme(int theme_id);
int vis_ui_theme_id(void);
void vis_ui_theme_next(VisConfig *cfg);
void vis_ui_theme_prev(VisConfig *cfg);
void vis_ui_render(struct SDL_Renderer *renderer);
void vis_ui_process_event(const SDL_Event *event);
bool vis_ui_want_capture_mouse(void);
bool vis_ui_want_capture_keyboard(void);
bool vis_ui_pick_mp3_file(char *out_path, size_t out_size);
bool vis_ui_pick_asset_file(char *out_path, size_t out_size);
bool vis_ui_pick_config_save(char *out_path, size_t out_size, const char *default_path);
bool vis_ui_pick_config_load(char *out_path, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
