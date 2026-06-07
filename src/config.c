#include "config.h"
#include "themes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vis_config_defaults(VisConfig *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->fps_cap = 60.0f;
  cfg->bg_anim_speed = 1.0f;
  cfg->asset_scale = 1.0f;
  cfg->asset_spin = 18.0f;
  cfg->asset_flip_on_beat = true;
  cfg->demo_mode = VIS_DEMO_OFF;
  vis_theme_apply(cfg, VIS_THEME_VU_METER);
}

const char *vis_bg_anim_name(VisBgAnim anim) {
  static const char *names[] = {"off",         "gradient_shift", "pulse",
                                "starfield",   "rainbow",        "rings"};
  if (anim < 0 || anim >= VIS_BG_COUNT) {
    return "off";
  }
  return names[anim];
}

const char *vis_bar_style_name(VisBarStyle style) {
  static const char *names[] = {"solid", "fluid", "cloud"};
  if (style < 0 || style >= VIS_BAR_STYLE_COUNT) {
    return "solid";
  }
  return names[style];
}

VisBarStyle vis_bar_style_from_name(const char *name) {
  if (!name) {
    return VIS_BAR_SOLID;
  }
  for (int i = 0; i < VIS_BAR_STYLE_COUNT; i++) {
    if (strcmp(name, vis_bar_style_name((VisBarStyle)i)) == 0) {
      return (VisBarStyle)i;
    }
  }
  return VIS_BAR_SOLID;
}

VisBgAnim vis_bg_anim_from_name(const char *name) {
  if (!name) {
    return VIS_BG_OFF;
  }
  if (strcmp(name, "beat_flash") == 0 || strcmp(name, "grid") == 0) {
    return VIS_BG_OFF;
  }
  for (int i = 0; i < VIS_BG_COUNT; i++) {
    if (strcmp(name, vis_bg_anim_name((VisBgAnim)i)) == 0) {
      return (VisBgAnim)i;
    }
  }
  return VIS_BG_OFF;
}

const char *vis_demo_mode_name(VisDemoMode mode) {
  static const char *names[] = {"off", "sorting"};
  if (mode < 0 || mode >= VIS_DEMO_COUNT) {
    return "off";
  }
  return names[mode];
}

VisDemoMode vis_demo_mode_from_name(const char *name) {
  if (!name) {
    return VIS_DEMO_OFF;
  }
  if (strcmp(name, "wave_path") == 0 || strcmp(name, "radial_graph") == 0 ||
      strcmp(name, "particle_field") == 0 || strcmp(name, "binary_tree") == 0) {
    return VIS_DEMO_OFF;
  }
  for (int i = 0; i < VIS_DEMO_COUNT; i++) {
    if (strcmp(name, vis_demo_mode_name((VisDemoMode)i)) == 0) {
      return (VisDemoMode)i;
    }
  }
  return VIS_DEMO_OFF;
}

const char *vis_layout_name(VisLayout layout) {
  static const char *names[] = {"vertical", "horizontal", "circular", "wave_path",
                                "radial_graph", "particles", "binary_tree"};
  if (layout < 0 || layout >= VIS_LAYOUT_COUNT) {
    return "vertical";
  }
  return names[layout];
}

bool vis_config_save_remembered_path(const char *path) {
  if (!path || path[0] == '\0') {
    return false;
  }
  FILE *f = fopen("visc.state.ini", "w");
  if (!f) {
    return false;
  }
  fprintf(f, "settings_path=%s\n", path);
  fclose(f);
  return true;
}

bool vis_config_load_remembered_path(char *out_path, size_t out_size) {
  if (!out_path || out_size == 0) {
    return false;
  }
  FILE *f = fopen("visc.state.ini", "r");
  if (!f) {
    return false;
  }
  char line[640];
  bool found = false;
  while (fgets(line, sizeof(line), f)) {
    char *nl = strchr(line, '\n');
    if (nl) {
      *nl = '\0';
    }
    char *eq = strchr(line, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    if (strcmp(line, "settings_path") == 0 && eq[1] != '\0') {
      snprintf(out_path, out_size, "%s", eq + 1);
      found = true;
      break;
    }
  }
  fclose(f);
  return found;
}

const char *vis_mode_name(VisMode mode) {
  static const char *names[] = {"spectrum", "waveform"};
  if (mode < 0 || mode >= VIS_MODE_COUNT) {
    return "spectrum";
  }
  return names[mode];
}

static void write_color(FILE *f, const char *key, const float c[4]) {
  fprintf(f, "%s=%.4f,%.4f,%.4f,%.4f\n", key, c[0], c[1], c[2], c[3]);
}

static bool read_color(const char *value, float c[4]) {
  return sscanf(value, "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]) == 4;
}

bool vis_config_save(const VisConfig *cfg, const char *path, int theme_id) {
  FILE *f = fopen(path, "w");
  if (!f) {
    return false;
  }
  fprintf(f, "# visc visualizer settings\n");
  if (theme_id >= 0 && theme_id < vis_theme_count()) {
    fprintf(f, "theme=%s\n", vis_theme_name((VisThemeId)theme_id));
  } else {
    fprintf(f, "theme=custom\n");
  }
  fprintf(f, "layout=%s\n", vis_layout_name(cfg->layout));
  fprintf(f, "mode=%s\n", vis_mode_name(cfg->mode));
  fprintf(f, "bar_count=%d\n", cfg->bar_count);
  fprintf(f, "bar_width_px=%.3f\n", cfg->bar_width_px);
  fprintf(f, "bar_gap_px=%.3f\n", cfg->bar_gap_px);
  write_color(f, "color_bg", cfg->color_bg);
  write_color(f, "color_bar", cfg->color_bar);
  write_color(f, "color_bar2", cfg->color_bar2);
  fprintf(f, "use_gradient=%d\n", cfg->use_gradient ? 1 : 0);
  fprintf(f, "sensitivity=%.4f\n", cfg->sensitivity);
  fprintf(f, "min_bar_norm=%.4f\n", cfg->min_bar_norm);
  fprintf(f, "smooth_attack=%.4f\n", cfg->smooth_attack);
  fprintf(f, "smooth_decay=%.4f\n", cfg->smooth_decay);
  fprintf(f, "freq_low_hz=%.2f\n", cfg->freq_low_hz);
  fprintf(f, "freq_high_hz=%.2f\n", cfg->freq_high_hz);
  fprintf(f, "mirrored=%d\n", cfg->mirrored ? 1 : 0);
  fprintf(f, "rounded_bars=%d\n", cfg->rounded_bars ? 1 : 0);
  fprintf(f, "show_peaks=%d\n", cfg->show_peaks ? 1 : 0);
  fprintf(f, "fps_cap=%.2f\n", cfg->fps_cap);
  fprintf(f, "transparent=%d\n", cfg->transparent ? 1 : 0);
  fprintf(f, "always_on_top=%d\n", cfg->always_on_top ? 1 : 0);
  fprintf(f, "party_mode=%d\n", cfg->party_mode ? 1 : 0);
  fprintf(f, "bg_anim=%s\n", vis_bg_anim_name(cfg->bg_anim));
  fprintf(f, "bg_anim_speed=%.3f\n", cfg->bg_anim_speed);
  fprintf(f, "show_stats=%d\n", cfg->show_stats ? 1 : 0);
  fprintf(f, "bar_style=%s\n", vis_bar_style_name(cfg->bar_style));
  fprintf(f, "asset_enabled=%d\n", cfg->asset_enabled ? 1 : 0);
  fprintf(f, "asset_path=%s\n", cfg->asset_path);
  fprintf(f, "asset_scale=%.3f\n", cfg->asset_scale);
  fprintf(f, "asset_spin=%.3f\n", cfg->asset_spin);
  fprintf(f, "asset_flip_on_beat=%d\n", cfg->asset_flip_on_beat ? 1 : 0);
  fprintf(f, "demo_mode=%s\n", vis_demo_mode_name(cfg->demo_mode));
  fclose(f);
  return true;
}

bool vis_config_load(VisConfig *cfg, const char *path, int *theme_id_out) {
  FILE *f = fopen(path, "r");
  if (!f) {
    return false;
  }
  VisConfig tmp;
  vis_config_defaults(&tmp);
  int loaded_theme = VIS_THEME_VU_METER;
  bool theme_applied = false;

  char line[512];
  while (fgets(line, sizeof(line), f)) {
    char *nl = strchr(line, '\n');
    if (nl) {
      *nl = '\0';
    }
    if (line[0] == '#' || line[0] == '\0') {
      continue;
    }
    char *eq = strchr(line, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    const char *key = line;
    const char *val = eq + 1;

    if (strcmp(key, "theme") == 0) {
      loaded_theme = vis_theme_from_name(val);
      if (loaded_theme >= 0 && loaded_theme < VIS_THEME_COUNT) {
        vis_theme_apply(&tmp, (VisThemeId)loaded_theme);
        theme_applied = true;
      } else {
        loaded_theme = VIS_THEME_CUSTOM;
      }
    } else if (strcmp(key, "layout") == 0) {
      for (int i = 0; i < VIS_LAYOUT_COUNT; i++) {
        if (strcmp(val, vis_layout_name((VisLayout)i)) == 0) {
          tmp.layout = (VisLayout)i;
          break;
        }
      }
    } else if (strcmp(key, "mode") == 0) {
      for (int i = 0; i < VIS_MODE_COUNT; i++) {
        if (strcmp(val, vis_mode_name((VisMode)i)) == 0) {
          tmp.mode = (VisMode)i;
          break;
        }
      }
    } else if (strcmp(key, "bar_count") == 0) {
      tmp.bar_count = atoi(val);
    } else if (strcmp(key, "bar_width_px") == 0) {
      tmp.bar_width_px = (float)atof(val);
    } else if (strcmp(key, "bar_gap_px") == 0) {
      tmp.bar_gap_px = (float)atof(val);
    } else if (strcmp(key, "color_bg") == 0) {
      read_color(val, tmp.color_bg);
    } else if (strcmp(key, "color_bar") == 0) {
      read_color(val, tmp.color_bar);
    } else if (strcmp(key, "color_bar2") == 0) {
      read_color(val, tmp.color_bar2);
    } else if (strcmp(key, "use_gradient") == 0) {
      tmp.use_gradient = atoi(val) != 0;
    } else if (strcmp(key, "sensitivity") == 0) {
      tmp.sensitivity = (float)atof(val);
    } else if (strcmp(key, "min_bar_norm") == 0) {
      tmp.min_bar_norm = (float)atof(val);
    } else if (strcmp(key, "smooth_attack") == 0) {
      tmp.smooth_attack = (float)atof(val);
    } else if (strcmp(key, "smooth_decay") == 0) {
      tmp.smooth_decay = (float)atof(val);
    } else if (strcmp(key, "freq_low_hz") == 0) {
      tmp.freq_low_hz = (float)atof(val);
    } else if (strcmp(key, "freq_high_hz") == 0) {
      tmp.freq_high_hz = (float)atof(val);
    } else if (strcmp(key, "mirrored") == 0) {
      tmp.mirrored = atoi(val) != 0;
    } else if (strcmp(key, "rounded_bars") == 0) {
      tmp.rounded_bars = atoi(val) != 0;
    } else if (strcmp(key, "show_peaks") == 0) {
      tmp.show_peaks = atoi(val) != 0;
    } else if (strcmp(key, "fps_cap") == 0) {
      tmp.fps_cap = (float)atof(val);
    } else if (strcmp(key, "transparent") == 0) {
      tmp.transparent = atoi(val) != 0;
    } else if (strcmp(key, "always_on_top") == 0) {
      tmp.always_on_top = atoi(val) != 0;
    } else if (strcmp(key, "party_mode") == 0) {
      tmp.party_mode = atoi(val) != 0;
    } else if (strcmp(key, "bg_anim") == 0) {
      tmp.bg_anim = vis_bg_anim_from_name(val);
    } else if (strcmp(key, "bg_anim_speed") == 0) {
      tmp.bg_anim_speed = (float)atof(val);
    } else if (strcmp(key, "show_stats") == 0) {
      tmp.show_stats = atoi(val) != 0;
    } else if (strcmp(key, "bar_style") == 0) {
      tmp.bar_style = vis_bar_style_from_name(val);
    } else if (strcmp(key, "asset_enabled") == 0) {
      tmp.asset_enabled = atoi(val) != 0;
    } else if (strcmp(key, "asset_path") == 0) {
      snprintf(tmp.asset_path, sizeof(tmp.asset_path), "%s", val);
    } else if (strcmp(key, "asset_scale") == 0) {
      tmp.asset_scale = (float)atof(val);
    } else if (strcmp(key, "asset_spin") == 0) {
      tmp.asset_spin = (float)atof(val);
    } else if (strcmp(key, "asset_flip_on_beat") == 0) {
      tmp.asset_flip_on_beat = atoi(val) != 0;
    } else if (strcmp(key, "demo_mode") == 0) {
      tmp.demo_mode = vis_demo_mode_from_name(val);
    }
  }
  fclose(f);
  if (!theme_applied) {
    loaded_theme = vis_theme_match(&tmp);
  }
  if (theme_id_out) {
    *theme_id_out = loaded_theme;
  }
  *cfg = tmp;
  if (cfg->bar_count < 8) {
    cfg->bar_count = 8;
  }
  if (cfg->bar_count > 256) {
    cfg->bar_count = 256;
  }
  if (cfg->asset_scale < 0.1f) {
    cfg->asset_scale = 0.1f;
  }
  if (cfg->asset_scale > 4.0f) {
    cfg->asset_scale = 4.0f;
  }
  return true;
}
