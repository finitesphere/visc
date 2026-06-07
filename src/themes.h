#ifndef VISC_THEMES_H
#define VISC_THEMES_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  VIS_THEME_AURORA = 0,
  VIS_THEME_SUNSET,
  VIS_THEME_NEON,
  VIS_THEME_FOREST,
  VIS_THEME_OCEAN,
  VIS_THEME_MONO,
  VIS_THEME_LAVA,
  VIS_THEME_MIDNIGHT,
  VIS_THEME_VU_METER,
  VIS_THEME_RADAR,
  VIS_THEME_RADIAL_GRAPH,
  VIS_THEME_WAVE_PATH,
  VIS_THEME_PARTICLES,
  VIS_THEME_COUNT
} VisThemeId;

#define VIS_THEME_CUSTOM (-1)

int vis_theme_count(void);
const char *vis_theme_name(VisThemeId id);
VisThemeId vis_theme_from_name(const char *name);

/* Apply preset visual settings (layout/mode can be changed per theme). */
void vis_theme_apply(VisConfig *cfg, VisThemeId id);

/* True if cfg matches a built-in preset (for UI highlight). */
int vis_theme_match(const VisConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif
