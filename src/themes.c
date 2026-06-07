#include "themes.h"

#include <math.h>
#include <string.h>

typedef struct {
  const char *name;
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
} VisThemePreset;

static const VisThemePreset k_themes[VIS_THEME_COUNT] = {
    {
        "Aurora",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        64,
        0.0f,
        2.0f,
        {0.04f, 0.05f, 0.08f, 1.0f},
        {0.20f, 0.75f, 1.00f, 1.0f},
        {0.95f, 0.25f, 0.85f, 1.0f},
        true,
        1.15f,
        0.02f,
        0.52f,
        0.12f,
        40.0f,
        16000.0f,
        false,
        true,
        true,
    },
    {
        "Sunset",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        72,
        0.0f,
        1.5f,
        {0.07f, 0.03f, 0.05f, 1.0f},
        {1.00f, 0.45f, 0.12f, 1.0f},
        {0.90f, 0.15f, 0.35f, 1.0f},
        true,
        1.20f,
        0.02f,
        0.54f,
        0.10f,
        35.0f,
        14000.0f,
        false,
        true,
        true,
    },
    {
        "Neon",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        48,
        0.0f,
        3.0f,
        {0.01f, 0.01f, 0.02f, 1.0f},
        {0.10f, 1.00f, 0.55f, 1.0f},
        {1.00f, 0.10f, 0.75f, 1.0f},
        true,
        1.35f,
        0.015f,
        0.58f,
        0.08f,
        50.0f,
        18000.0f,
        false,
        false,
        true,
    },
    {
        "Forest",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        56,
        0.0f,
        2.0f,
        {0.03f, 0.06f, 0.04f, 1.0f},
        {0.25f, 0.85f, 0.35f, 1.0f},
        {0.70f, 0.95f, 0.30f, 1.0f},
        true,
        1.10f,
        0.02f,
        0.46f,
        0.14f,
        30.0f,
        12000.0f,
        false,
        true,
        false,
    },
    {
        "Ocean",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        80,
        0.0f,
        1.0f,
        {0.02f, 0.05f, 0.10f, 1.0f},
        {0.15f, 0.55f, 0.95f, 1.0f},
        {0.20f, 0.90f, 0.85f, 1.0f},
        true,
        1.12f,
        0.018f,
        0.50f,
        0.13f,
        25.0f,
        15000.0f,
        true,
        true,
        true,
    },
    {
        "Mono",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        40,
        0.0f,
        4.0f,
        {0.06f, 0.06f, 0.06f, 1.0f},
        {0.92f, 0.92f, 0.92f, 1.0f},
        {0.55f, 0.55f, 0.55f, 1.0f},
        true,
        1.00f,
        0.01f,
        0.42f,
        0.15f,
        40.0f,
        16000.0f,
        false,
        false,
        false,
    },
    {
        "Lava",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        64,
        0.0f,
        2.0f,
        {0.05f, 0.02f, 0.01f, 1.0f},
        {1.00f, 0.20f, 0.05f, 1.0f},
        {1.00f, 0.85f, 0.10f, 1.0f},
        true,
        1.25f,
        0.025f,
        0.56f,
        0.09f,
        45.0f,
        12000.0f,
        true,
        true,
        true,
    },
    {
        "Midnight",
        VIS_LAYOUT_VERTICAL,
        VIS_MODE_SPECTRUM,
        96,
        0.0f,
        1.0f,
        {0.02f, 0.02f, 0.05f, 1.0f},
        {0.45f, 0.30f, 0.95f, 1.0f},
        {0.75f, 0.35f, 0.90f, 1.0f},
        true,
        1.05f,
        0.015f,
        0.48f,
        0.11f,
        35.0f,
        17000.0f,
        false,
        true,
        true,
    },
    {
        "VU Meter",
        VIS_LAYOUT_HORIZONTAL,
        VIS_MODE_WAVEFORM,
        32,
        0.0f,
        3.0f,
        {0.08f, 0.08f, 0.07f, 1.0f},
        {0.20f, 0.90f, 0.25f, 1.0f},
        {0.95f, 0.25f, 0.15f, 1.0f},
        true,
        1.45f,
        0.03f,
        0.60f,
        0.06f,
        60.0f,
        8000.0f,
        false,
        false,
        true,
    },
    {
        "Radar",
        VIS_LAYOUT_CIRCULAR,
        VIS_MODE_SPECTRUM,
        88,
        3.0f,
        0.0f,
        {0.01f, 0.04f, 0.03f, 1.0f},
        {0.20f, 1.00f, 0.70f, 1.0f},
        {0.10f, 0.50f, 1.00f, 1.0f},
        true,
        1.20f,
        0.02f,
        0.54f,
        0.10f,
        50.0f,
        16000.0f,
        false,
        false,
        true,
    },
    {
        "Orbit Bloom",
        VIS_LAYOUT_RADIAL_GRAPH,
        VIS_MODE_SPECTRUM,
        96,
        0.0f,
        1.0f,
        {0.025f, 0.025f, 0.035f, 1.0f},
        {0.25f, 0.92f, 0.80f, 1.0f},
        {0.95f, 0.45f, 0.35f, 1.0f},
        true,
        1.15f,
        0.015f,
        0.50f,
        0.10f,
        35.0f,
        16000.0f,
        false,
        false,
        false,
    },
    {
        "Signal Tide",
        VIS_LAYOUT_WAVE_PATH,
        VIS_MODE_WAVEFORM,
        96,
        0.0f,
        1.0f,
        {0.02f, 0.035f, 0.045f, 1.0f},
        {0.20f, 0.78f, 0.95f, 1.0f},
        {0.95f, 0.86f, 0.32f, 1.0f},
        true,
        1.30f,
        0.02f,
        0.56f,
        0.08f,
        45.0f,
        12000.0f,
        false,
        false,
        false,
    },
    {
        "Prism Dust",
        VIS_LAYOUT_PARTICLES,
        VIS_MODE_SPECTRUM,
        128,
        0.0f,
        1.0f,
        {0.025f, 0.018f, 0.030f, 1.0f},
        {0.92f, 0.38f, 0.88f, 1.0f},
        {0.35f, 0.95f, 0.58f, 1.0f},
        true,
        1.25f,
        0.012f,
        0.58f,
        0.08f,
        55.0f,
        18000.0f,
        false,
        false,
        false,
    },
};

static void copy_color(float dst[4], const float src[4]) {
  memcpy(dst, src, sizeof(float) * 4);
}

int vis_theme_count(void) { return VIS_THEME_COUNT; }

const char *vis_theme_name(VisThemeId id) {
  if (id < 0 || id >= VIS_THEME_COUNT) {
    return "Custom";
  }
  return k_themes[id].name;
}

VisThemeId vis_theme_from_name(const char *name) {
  if (!name || name[0] == '\0') {
    return VIS_THEME_VU_METER;
  }
  if (strcmp(name, "custom") == 0) {
    return VIS_THEME_CUSTOM;
  }
  for (int i = 0; i < VIS_THEME_COUNT; i++) {
    if (strcmp(name, k_themes[i].name) == 0) {
      return (VisThemeId)i;
    }
  }
  return VIS_THEME_CUSTOM;
}

void vis_theme_apply(VisConfig *cfg, VisThemeId id) {
  if (!cfg || id < 0 || id >= VIS_THEME_COUNT) {
    return;
  }
  const VisThemePreset *t = &k_themes[id];
  cfg->layout = t->layout;
  cfg->mode = t->mode;
  cfg->bar_count = t->bar_count;
  cfg->bar_width_px = t->bar_width_px;
  cfg->bar_gap_px = t->bar_gap_px;
  copy_color(cfg->color_bg, t->color_bg);
  copy_color(cfg->color_bar, t->color_bar);
  copy_color(cfg->color_bar2, t->color_bar2);
  cfg->use_gradient = t->use_gradient;
  cfg->sensitivity = t->sensitivity;
  cfg->min_bar_norm = t->min_bar_norm;
  cfg->smooth_attack = t->smooth_attack;
  cfg->smooth_decay = t->smooth_decay;
  cfg->freq_low_hz = t->freq_low_hz;
  cfg->freq_high_hz = t->freq_high_hz;
  cfg->mirrored = t->mirrored;
  cfg->rounded_bars = t->rounded_bars;
  cfg->show_peaks = t->show_peaks;
}

static bool feq(float a, float b) { return fabsf(a - b) < 0.0005f; }

static bool color_eq(const float a[4], const float b[4]) {
  return feq(a[0], b[0]) && feq(a[1], b[1]) && feq(a[2], b[2]) && feq(a[3], b[3]);
}

int vis_theme_match(const VisConfig *cfg) {
  if (!cfg) {
    return VIS_THEME_CUSTOM;
  }
  for (int i = 0; i < VIS_THEME_COUNT; i++) {
    const VisThemePreset *t = &k_themes[i];
    if (cfg->layout != t->layout || cfg->mode != t->mode || cfg->bar_count != t->bar_count) {
      continue;
    }
    if (!feq(cfg->bar_width_px, t->bar_width_px) || !feq(cfg->bar_gap_px, t->bar_gap_px)) {
      continue;
    }
    if (!color_eq(cfg->color_bg, t->color_bg) || !color_eq(cfg->color_bar, t->color_bar) ||
        !color_eq(cfg->color_bar2, t->color_bar2)) {
      continue;
    }
    if (cfg->use_gradient != t->use_gradient || cfg->mirrored != t->mirrored ||
        cfg->rounded_bars != t->rounded_bars || cfg->show_peaks != t->show_peaks) {
      continue;
    }
    if (!feq(cfg->sensitivity, t->sensitivity) || !feq(cfg->min_bar_norm, t->min_bar_norm) ||
        !feq(cfg->smooth_attack, t->smooth_attack) || !feq(cfg->smooth_decay, t->smooth_decay)) {
      continue;
    }
    if (!feq(cfg->freq_low_hz, t->freq_low_hz) || !feq(cfg->freq_high_hz, t->freq_high_hz)) {
      continue;
    }
    return i;
  }
  return VIS_THEME_CUSTOM;
}
