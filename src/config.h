#ifndef VISC_CONFIG_H
#define VISC_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VIS_LAYOUT_VERTICAL = 0,
  VIS_LAYOUT_HORIZONTAL = 1,
  VIS_LAYOUT_CIRCULAR = 2,
  VIS_LAYOUT_WAVE_PATH = 3,
  VIS_LAYOUT_RADIAL_GRAPH = 4,
  VIS_LAYOUT_PARTICLES = 5,
  VIS_LAYOUT_BINARY_TREE = 6,
  VIS_LAYOUT_COUNT
} VisLayout;

typedef enum {
  VIS_MODE_SPECTRUM = 0,
  VIS_MODE_WAVEFORM = 1,
  VIS_MODE_COUNT
} VisMode;

typedef enum {
  VIS_BG_OFF = 0,
  VIS_BG_GRADIENT_SHIFT,
  VIS_BG_PULSE,
  VIS_BG_STARFIELD,
  VIS_BG_RAINBOW,
  VIS_BG_RINGS,
  VIS_BG_COUNT
} VisBgAnim;

typedef enum {
  VIS_BAR_SOLID = 0,
  VIS_BAR_FLUID,
  VIS_BAR_CLOUD,
  VIS_BAR_STYLE_COUNT
} VisBarStyle;

typedef enum {
  VIS_DEMO_OFF = 0,
  VIS_DEMO_SORTING,
  VIS_DEMO_COUNT
} VisDemoMode;

typedef struct {
  VisLayout layout;
  VisMode mode;

  int bar_count;
  float bar_width_px;
  float bar_gap_px;

  float color_bg[4];
  float color_bar[4];
  float color_bar2[4];
  bool use_gradient;

  float sensitivity;
  float min_bar_norm;
  float smooth_attack;
  float smooth_decay;

  float freq_low_hz;
  float freq_high_hz;

  bool mirrored;
  bool rounded_bars;
  bool show_peaks;

  float fps_cap;

  bool transparent;
  bool always_on_top;
  bool party_mode;
  VisBgAnim bg_anim;
  float bg_anim_speed;

  bool show_stats;
  VisBarStyle bar_style;

  bool asset_enabled;
  char asset_path[512];
  float asset_scale;
  float asset_spin;
  bool asset_flip_on_beat;

  VisDemoMode demo_mode;
} VisConfig;

void vis_config_defaults(VisConfig *cfg);
bool vis_config_save(const VisConfig *cfg, const char *path, int theme_id);
bool vis_config_load(VisConfig *cfg, const char *path, int *theme_id_out);
bool vis_config_save_remembered_path(const char *path);
bool vis_config_load_remembered_path(char *out_path, size_t out_size);

const char *vis_layout_name(VisLayout layout);
const char *vis_mode_name(VisMode mode);
const char *vis_bg_anim_name(VisBgAnim anim);
VisBgAnim vis_bg_anim_from_name(const char *name);
const char *vis_bar_style_name(VisBarStyle style);
VisBarStyle vis_bar_style_from_name(const char *name);
const char *vis_demo_mode_name(VisDemoMode mode);
VisDemoMode vis_demo_mode_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif
