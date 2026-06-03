#ifndef VISC_CONFIG_H
#define VISC_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VIS_LAYOUT_VERTICAL = 0,
  VIS_LAYOUT_HORIZONTAL = 1,
  VIS_LAYOUT_CIRCULAR = 2,
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
  VIS_BG_BEAT_FLASH,
  VIS_BG_COUNT
} VisBgAnim;

typedef enum {
  VIS_BAR_SOLID = 0,
  VIS_BAR_FLUID,
  VIS_BAR_CLOUD,
  VIS_BAR_STYLE_COUNT
} VisBarStyle;

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
  bool bar_glow;
  bool party_mode;
  VisBgAnim bg_anim;
  float bg_anim_speed;

  bool show_stats;
  VisBarStyle bar_style;
} VisConfig;

void vis_config_defaults(VisConfig *cfg);
bool vis_config_save(const VisConfig *cfg, const char *path, int theme_id);
bool vis_config_load(VisConfig *cfg, const char *path, int *theme_id_out);

const char *vis_layout_name(VisLayout layout);
const char *vis_mode_name(VisMode mode);
const char *vis_bg_anim_name(VisBgAnim anim);
VisBgAnim vis_bg_anim_from_name(const char *name);
const char *vis_bar_style_name(VisBarStyle style);
VisBarStyle vis_bar_style_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif
