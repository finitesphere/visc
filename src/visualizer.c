#include "visualizer.h"
#include "image_loader.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STAR_COUNT 120

typedef struct {
  float x, y, z, speed;
} Star;

static Star g_stars[STAR_COUNT];
static bool g_stars_init = false;
static SDL_Texture *g_asset_texture = NULL;
static char g_asset_path[512] = "";
static int g_asset_w = 0;
static int g_asset_h = 0;

static void init_stars(void) {
  if (g_stars_init) {
    return;
  }
  for (int i = 0; i < STAR_COUNT; i++) {
    g_stars[i].x = (float)rand() / (float)RAND_MAX;
    g_stars[i].y = (float)rand() / (float)RAND_MAX;
    g_stars[i].z = 0.2f + 0.8f * ((float)rand() / (float)RAND_MAX);
    g_stars[i].speed = 0.15f + 0.5f * ((float)rand() / (float)RAND_MAX);
  }
  g_stars_init = true;
}

static SDL_Color color_from_norm(const float c[4]) {
  SDL_Color out;
  out.r = (Uint8)(fminf(fmaxf(c[0], 0.0f), 1.0f) * 255.0f);
  out.g = (Uint8)(fminf(fmaxf(c[1], 0.0f), 1.0f) * 255.0f);
  out.b = (Uint8)(fminf(fmaxf(c[2], 0.0f), 1.0f) * 255.0f);
  out.a = (Uint8)(fminf(fmaxf(c[3], 0.0f), 1.0f) * 255.0f);
  return out;
}

static void lerp_color(float out[4], const float a[4], const float b[4], float t) {
  for (int i = 0; i < 4; i++) {
    out[i] = a[i] + (b[i] - a[i]) * t;
  }
}

static SDL_Color bar_color(const VisConfig *cfg, float t) {
  float mix[4];
  if (cfg->use_gradient) {
    lerp_color(mix, cfg->color_bar, cfg->color_bar2, t);
  } else {
    memcpy(mix, cfg->color_bar, sizeof(mix));
  }
  return color_from_norm(mix);
}

static float analysis_energy(const VisAnalysis *analysis, int bars) {
  float sum = 0.0f;
  for (int i = 0; i < bars; i++) {
    sum += analysis->bars[i];
  }
  return sum / (float)bars;
}

static void peak_features(const VisAnalysis *analysis, int bars, float *level, float *freq_t) {
  int peak_i = 0;
  float peak_v = 0.0f;
  for (int i = 0; i < bars; i++) {
    if (analysis->bars[i] > peak_v) {
      peak_v = analysis->bars[i];
      peak_i = i;
    }
  }
  if (level) {
    *level = fminf(fmaxf(peak_v, 0.0f), 1.0f);
  }
  if (freq_t) {
    *freq_t = bars > 1 ? (float)peak_i / (float)(bars - 1) : 0.0f;
  }
}

static void unload_asset(void) {
  if (g_asset_texture) {
    SDL_DestroyTexture(g_asset_texture);
    g_asset_texture = NULL;
  }
  g_asset_path[0] = '\0';
  g_asset_w = 0;
  g_asset_h = 0;
}

static bool ensure_asset(SDL_Renderer *renderer, const char *path) {
  if (!path || path[0] == '\0') {
    unload_asset();
    return false;
  }
  if (g_asset_texture && strcmp(g_asset_path, path) == 0) {
    return true;
  }
  unload_asset();
  SDL_Surface *surface = vis_image_load_surface(path);
  if (!surface) {
    return false;
  }
  SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
  g_asset_texture = SDL_CreateTextureFromSurface(renderer, surface);
  g_asset_w = surface->w;
  g_asset_h = surface->h;
  SDL_FreeSurface(surface);
  if (!g_asset_texture) {
    return false;
  }
  SDL_SetTextureBlendMode(g_asset_texture, SDL_BLENDMODE_BLEND);
  snprintf(g_asset_path, sizeof(g_asset_path), "%s", path);
  return true;
}

static void clear_frame(SDL_Renderer *renderer, const VisConfig *cfg) {
  if (cfg->transparent) {
    /* Windows color-key transparency: only pure black is see-through. */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  } else {
    SDL_Color bg = color_from_norm(cfg->color_bg);
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
  }
  SDL_RenderClear(renderer);
}

static void draw_solid_bg(SDL_Renderer *renderer, const float rgba[4]) {
  SDL_Color c = color_from_norm(rgba);
  SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
  SDL_RenderClear(renderer);
}

static void fill_circle(SDL_Renderer *renderer, float cx, float cy, float r, SDL_Color col) {
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  int radius = (int)ceilf(r);
  for (int y = -radius; y <= radius; y++) {
    float yf = (float)y;
    float half = sqrtf(fmaxf(r * r - yf * yf, 0.0f));
    SDL_RenderDrawLineF(renderer, cx - half, cy + yf, cx + half, cy + yf);
  }
}

static void draw_circle_outline(SDL_Renderer *renderer, float cx, float cy, float r, SDL_Color col) {
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  const int steps = 96;
  float prev_x = cx + r;
  float prev_y = cy;
  for (int i = 1; i <= steps; i++) {
    float a = (float)(2.0 * M_PI * i / steps);
    float x = cx + cosf(a) * r;
    float y = cy + sinf(a) * r;
    SDL_RenderDrawLineF(renderer, prev_x, prev_y, x, y);
    prev_x = x;
    prev_y = y;
  }
}

static void draw_animated_bg(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                             const VisAnalysis *analysis, float time_sec) {
  if (cfg->transparent && cfg->bg_anim == VIS_BG_OFF) {
    return;
  }

  float speed = cfg->bg_anim_speed > 0.05f ? cfg->bg_anim_speed : 1.0f;
  float t = time_sec * speed;
  int bars = cfg->bar_count;
  float energy = analysis_energy(analysis, bars);

  float bg[4];
  memcpy(bg, cfg->color_bg, sizeof(bg));

  switch (cfg->bg_anim) {
  case VIS_BG_GRADIENT_SHIFT: {
    float shift = 0.5f + 0.5f * sinf(t * 0.7f);
    float alt[4] = {cfg->color_bar[0] * 0.15f, cfg->color_bar[1] * 0.15f, cfg->color_bar[2] * 0.15f,
                    cfg->transparent ? 0.35f : 1.0f};
    lerp_color(bg, cfg->color_bg, alt, shift * 0.45f);
    if (cfg->transparent) {
      bg[3] = 0.25f + 0.15f * shift;
    }
    draw_solid_bg(renderer, bg);
    break;
  }
  case VIS_BG_PULSE: {
    float pulse = 0.65f + 0.35f * fminf(energy * 2.5f, 1.0f);
    for (int i = 0; i < 3; i++) {
      bg[i] = cfg->color_bg[i] * pulse + cfg->color_bar[i] * (1.0f - pulse) * 0.25f;
    }
    bg[3] = cfg->transparent ? 0.2f + 0.25f * energy : 1.0f;
    draw_solid_bg(renderer, bg);
    break;
  }
  case VIS_BG_STARFIELD: {
    if (!cfg->transparent) {
      draw_solid_bg(renderer, cfg->color_bg);
    } else {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
      SDL_RenderClear(renderer);
    }
    init_stars();
    for (int i = 0; i < STAR_COUNT; i++) {
      Star *s = &g_stars[i];
      s->y += s->speed * 0.002f * speed;
      if (s->y > 1.05f) {
        s->y = -0.05f;
        s->x = (float)rand() / (float)RAND_MAX;
      }
      int alpha = (int)(180.0f * s->z);
      if (cfg->transparent) {
        alpha = (int)(120.0f * s->z);
      }
      SDL_SetRenderDrawColor(renderer, 220, 230, 255, (Uint8)alpha);
      float px = s->x * (float)w;
      float py = s->y * (float)h;
      float sz = 1.0f + 2.5f * s->z;
      SDL_FRect dot = {px, py, sz, sz};
      SDL_RenderFillRectF(renderer, &dot);
    }
    break;
  }
  case VIS_BG_RAINBOW: {
    int bands = 32;
    float band_w = (float)w / (float)bands;
    for (int i = 0; i < bands; i++) {
      float hue = fmodf((float)i / (float)bands + t * 0.08f, 1.0f);
      float r = fabsf(hue * 6.0f - 3.0f) - 1.0f;
      float g = 2.0f - fabsf(hue * 6.0f - 2.0f);
      float b = 2.0f - fabsf(hue * 6.0f - 4.0f);
      r = fmaxf(0.0f, fminf(r, 1.0f)) * 0.12f;
      g = fmaxf(0.0f, fminf(g, 1.0f)) * 0.12f;
      b = fmaxf(0.0f, fminf(b, 1.0f)) * 0.12f;
      Uint8 a = cfg->transparent ? 40 : 200;
      SDL_SetRenderDrawColor(renderer, (Uint8)(r * 255), (Uint8)(g * 255), (Uint8)(b * 255), a);
      SDL_FRect band = {i * band_w, 0.0f, band_w + 1.0f, (float)h};
      SDL_RenderFillRectF(renderer, &band);
    }
    break;
  }
  case VIS_BG_RINGS: {
    draw_solid_bg(renderer, cfg->color_bg);
    float peak = 0.0f;
    float freq_t = 0.0f;
    peak_features(analysis, bars, &peak, &freq_t);
    SDL_Color a = bar_color(cfg, freq_t);
    SDL_Color b = bar_color(cfg, 1.0f - freq_t * 0.65f);
    float cx = (float)w * 0.5f;
    float cy = (float)h * 0.5f;
    float orbit = fminf((float)w, (float)h) * 0.10f * peak;
    cx += cosf(freq_t * (float)(M_PI * 2.0) + t) * orbit;
    cy += sinf(freq_t * (float)(M_PI * 2.0) + t) * orbit;
    float max_r = fminf((float)w, (float)h) * (0.35f + 0.35f * peak);
    int rings = 5 + (int)(peak * 7.0f);
    for (int i = 0; i < rings; i++) {
      float p = fmodf(t * (0.10f + peak * 0.34f) + (float)i / (float)rings + freq_t * 0.35f, 1.0f);
      SDL_Color col = (i % 2) ? a : b;
      col.a = (Uint8)(45.0f + 185.0f * peak * (1.0f - p));
      float radius = 20.0f + p * max_r + sinf(t * 2.0f + freq_t * 8.0f) * peak * 18.0f;
      draw_circle_outline(renderer, cx, cy, radius, col);
      if (peak > 0.35f) {
        col.a = (Uint8)(col.a * 0.35f);
        draw_circle_outline(renderer, cx, cy, radius + 3.0f + peak * 10.0f, col);
      }
    }
    break;
  }
  case VIS_BG_OFF:
  default:
    if (!cfg->transparent) {
      draw_solid_bg(renderer, cfg->color_bg);
    }
    break;
  }
}

static float g_fluid_h[VISC_MAX_BARS];
static float g_fluid_v[VISC_MAX_BARS];

static int clamp_bar_count(int bars) {
  if (bars < 8) {
    return 8;
  }
  if (bars > VISC_MAX_BARS) {
    return VISC_MAX_BARS;
  }
  return bars;
}

static int bars_that_fit(int w, const VisConfig *cfg, float *bw_out, float *gap_out) {
  int bars = clamp_bar_count(cfg->bar_count);
  float gap = cfg->bar_gap_px;
  float bw = cfg->bar_width_px;
  if (bw <= 0.0f) {
    bw = ((float)w - gap * (float)(bars + 1)) / (float)bars;
    if (bw < 1.0f) {
      bw = 1.0f;
    }
  }
  while (bars > 1 && gap + (float)bars * (bw + gap) > (float)w) {
    bars--;
  }
  if (bw_out) {
    *bw_out = bw;
  }
  if (gap_out) {
    *gap_out = gap;
  }
  return bars;
}

static void fluid_step(const VisAnalysis *analysis, int bars, float dt) {
  for (int i = 0; i < bars; i++) {
    float target = analysis->bars[i];
    float force = (target - g_fluid_h[i]) * 14.0f;
    g_fluid_v[i] += force * dt;
    g_fluid_v[i] *= 0.90f;
    g_fluid_h[i] += g_fluid_v[i] * dt;
    if (g_fluid_h[i] < 0.0f) {
      g_fluid_h[i] = 0.0f;
    }
    if (g_fluid_h[i] > 1.2f) {
      g_fluid_h[i] = 1.2f;
    }
  }
}

static void draw_fluid_column(SDL_Renderer *renderer, float x, float base_y, float max_h, float bw,
                              float level, SDL_Color col, float time_sec, int index, bool grow_up) {
  float h = level * max_h;
  if (h < 2.0f) {
    return;
  }
  int blobs = (int)(h / 5.0f) + 2;
  for (int b = 0; b < blobs; b++) {
    float y = grow_up ? base_y - (float)(b + 1) * 5.0f : base_y + (float)(b + 1) * 5.0f;
    if (grow_up && y < base_y - h) {
      continue;
    }
    if (!grow_up && y > base_y + h) {
      continue;
    }
    float wob = sinf(time_sec * 2.8f + (float)index * 0.42f + (float)b * 0.18f) * (bw * 0.12f);
    Uint8 alpha = (Uint8)fmaxf(80.0f, 220.0f - (float)b * 8.0f);
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, alpha);
    SDL_FRect blob = {x + wob, y, bw * 0.92f, 5.5f};
    SDL_RenderFillRectF(renderer, &blob);
  }
}

static void draw_cloud_column(SDL_Renderer *renderer, float x, float base_y, float max_h, float bw,
                              float level, SDL_Color col, float time_sec, int index, bool grow_up) {
  float h = level * max_h;
  if (h < 4.0f) {
    return;
  }
  int puffs = (int)(h / 14.0f) + 3;
  for (int p = 0; p < puffs; p++) {
    float t = (float)p / (float)(puffs - 1 > 0 ? puffs - 1 : 1);
    float y = grow_up ? base_y - h * t : base_y + h * t;
    float drift = sinf(time_sec * 1.6f + (float)index * 0.25f + t * 3.0f) * (bw * 0.2f);
    float radius = bw * (0.55f + 0.2f * sinf((float)p + time_sec));
    Uint8 alpha = (Uint8)(140.0f + 80.0f * (1.0f - t));
    SDL_Color puff_col = col;
    puff_col.a = alpha;
    fill_circle(renderer, x + drift + bw * 0.35f, y, radius * 0.65f, puff_col);
    puff_col.a = (Uint8)(alpha * 0.75f);
    fill_circle(renderer, x + drift + bw * 0.62f, y - radius * 0.12f, radius * 0.48f, puff_col);
  }
}

static void fill_bar(SDL_Renderer *renderer, const VisConfig *cfg, SDL_FRect rect, SDL_Color col) {
  (void)cfg;
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  SDL_RenderFillRectF(renderer, &rect);
}

static int draw_vertical(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                         const VisAnalysis *analysis, float time_sec, float dt_sec) {
  float gap = 0.0f;
  float bw = 0.0f;
  int bars = bars_that_fit(w, cfg, &bw, &gap);

  float base_y = (float)h;
  float max_h = (float)h;
  if (cfg->mirrored) {
    base_y = (float)h * 0.5f;
    max_h = base_y;
  }

  if (cfg->bar_style == VIS_BAR_FLUID || cfg->bar_style == VIS_BAR_CLOUD) {
    fluid_step(analysis, bars, dt_sec);
  }

  for (int i = 0; i < bars; i++) {
    float x = gap + (float)i * (bw + gap);
    float norm = analysis->bars[i];
    if (norm < cfg->min_bar_norm) {
      norm = cfg->min_bar_norm;
    }
    float level = norm;
    if (cfg->bar_style == VIS_BAR_FLUID || cfg->bar_style == VIS_BAR_CLOUD) {
      level = g_fluid_h[i];
    }
    SDL_Color col = bar_color(cfg, bars > 1 ? (float)i / (float)(bars - 1) : 0.0f);

    if (cfg->bar_style == VIS_BAR_FLUID) {
      draw_fluid_column(renderer, x, base_y, max_h, bw, level, col, time_sec, i, true);
      if (cfg->mirrored) {
        draw_fluid_column(renderer, x, base_y, max_h, bw, level, col, time_sec + 1.7f, i, false);
      }
    } else if (cfg->bar_style == VIS_BAR_CLOUD) {
      draw_cloud_column(renderer, x, base_y, max_h, bw, level, col, time_sec, i, true);
      if (cfg->mirrored) {
        draw_cloud_column(renderer, x, base_y, max_h, bw, level, col, time_sec + 2.3f, i, false);
      }
    } else {
      SDL_FRect rect = {x, base_y - level * max_h, bw, level * max_h};
      fill_bar(renderer, cfg, rect, col);
      if (cfg->mirrored) {
        SDL_FRect mir = {x, base_y, bw, level * max_h};
        fill_bar(renderer, cfg, mir, col);
      }
    }

    if (cfg->show_peaks) {
      float py = base_y - analysis->peaks[i] * max_h;
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 210);
      SDL_FRect peak = {x, py - 2.0f, bw, 3.0f};
      SDL_RenderFillRectF(renderer, &peak);
    }
  }
  return bars;
}

static void draw_horizontal(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                            const VisAnalysis *analysis) {
  int bars = clamp_bar_count(cfg->bar_count);
  float gap = cfg->bar_gap_px;
  float bh = cfg->bar_width_px;
  if (bh <= 0.0f) {
    bh = ((float)h - gap * (float)(bars + 1)) / (float)bars;
    if (bh < 1.0f) {
      bh = 1.0f;
    }
  }
  float base_x = 0.0f;
  float max_w = (float)w;
  if (cfg->mirrored) {
    base_x = (float)w * 0.5f;
    max_w = base_x;
  }

  for (int i = 0; i < bars; i++) {
    float y = gap + (float)i * (bh + gap);
    float norm = analysis->bars[i];
    if (norm < cfg->min_bar_norm) {
      norm = cfg->min_bar_norm;
    }
    SDL_Color col = bar_color(cfg, bars > 1 ? (float)i / (float)(bars - 1) : 0.0f);
    SDL_FRect rect = {base_x, y, norm * max_w, bh};
    fill_bar(renderer, cfg, rect, col);
    if (cfg->mirrored) {
      SDL_FRect mir = {base_x - norm * max_w, y, norm * max_w, bh};
      fill_bar(renderer, cfg, mir, col);
    }
  }
}

static void draw_circular(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                          const VisAnalysis *analysis) {
  int bars = clamp_bar_count(cfg->bar_count);
  float cx = (float)w * 0.5f;
  float cy = (float)h * 0.5f;
  float inner = fminf((float)w, (float)h) * 0.18f;
  float max_len = fminf((float)w, (float)h) * 0.38f;

  for (int i = 0; i < bars; i++) {
    float ang = (float)(2.0 * M_PI * i / bars) - (float)M_PI / 2.0f;
    float norm = analysis->bars[i];
    if (norm < cfg->min_bar_norm) {
      norm = cfg->min_bar_norm;
    }
    float len = norm * max_len;
    SDL_Color col = bar_color(cfg, bars > 1 ? (float)i / (float)(bars - 1) : 0.0f);
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    float thick = cfg->bar_width_px > 0.0f ? cfg->bar_width_px : 3.0f;
    for (float t = 0.0f; t < thick; t += 1.0f) {
      float r0 = inner + t;
      float r1 = inner + len + t;
      float x0 = cx + cosf(ang) * r0;
      float y0 = cy + sinf(ang) * r0;
      float x1 = cx + cosf(ang) * r1;
      float y1 = cy + sinf(ang) * r1;
      SDL_RenderDrawLineF(renderer, x0, y0, x1, y1);
    }
  }
}

static void draw_reactive_asset(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                                const VisAnalysis *analysis, float time_sec) {
  if (!cfg->asset_enabled || !ensure_asset(renderer, cfg->asset_path)) {
    return;
  }
  float energy = analysis_energy(analysis, clamp_bar_count(cfg->bar_count));
  float scale = cfg->asset_scale * (0.75f + fminf(energy * 1.4f, 0.9f));
  float dst_w = (float)g_asset_w * scale;
  float dst_h = (float)g_asset_h * scale;
  if (dst_w > (float)w * 0.9f) {
    float s = ((float)w * 0.9f) / dst_w;
    dst_w *= s;
    dst_h *= s;
  }
  if (dst_h > (float)h * 0.9f) {
    float s = ((float)h * 0.9f) / dst_h;
    dst_w *= s;
    dst_h *= s;
  }
  SDL_FRect dst = {((float)w - dst_w) * 0.5f, ((float)h - dst_h) * 0.5f, dst_w, dst_h};
  double angle = (double)(time_sec * cfg->asset_spin + energy * 80.0f);
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  if (cfg->asset_flip_on_beat && energy > 0.42f) {
    flip = SDL_FLIP_HORIZONTAL;
  }
  SDL_RenderCopyExF(renderer, g_asset_texture, NULL, &dst, angle, NULL, flip);
}

static void draw_sorting_demo(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                              float time_sec) {
  enum { count = 48 };
  static float values[count];
  static bool init = false;
  static int pass = 0;
  static int scan = 0;
  static float last_time = 0.0f;
  static float accum = 0.0f;

  if (!init) {
    for (int i = 0; i < count; i++) {
      int v = (i * 37 + 17) % count;
      values[i] = 0.08f + 0.92f * ((float)(v + 1) / (float)count);
    }
    init = true;
    last_time = time_sec;
  }

  float dt = time_sec - last_time;
  if (dt < 0.0f || dt > 0.25f) {
    dt = 0.016f;
  }
  last_time = time_sec;
  accum += dt;
  while (accum >= 0.035f) {
    int limit = count - pass - 1;
    if (limit <= 0) {
      init = false;
      break;
    }
    if (values[scan] > values[scan + 1]) {
      float tmp = values[scan];
      values[scan] = values[scan + 1];
      values[scan + 1] = tmp;
    }
    scan++;
    if (scan >= limit) {
      scan = 0;
      pass++;
    }
    accum -= 0.035f;
  }

  float area_w = fminf((float)w * 0.82f, 820.0f);
  float area_h = fminf((float)h * 0.62f, 420.0f);
  float start_x = ((float)w - area_w) * 0.5f;
  float base_y = ((float)h + area_h) * 0.5f;
  float bw = area_w / (float)count;
  int active = scan;
  for (int i = 0; i < count; i++) {
    float value = values[i];
    SDL_Color col = bar_color(cfg, (float)i / (float)(count - 1));
    if (i == active || i == active + 1) {
      col.r = 255;
      col.g = 245;
      col.b = 170;
    }
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 210);
    SDL_FRect rect = {start_x + (float)i * bw + 1.0f, base_y - value * area_h, bw - 2.0f,
                      value * area_h};
    SDL_RenderFillRectF(renderer, &rect);
  }
}

static void draw_wave_path_demo(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                                const VisAnalysis *analysis, float time_sec) {
  int points = clamp_bar_count(cfg->bar_count);
  float x0 = (float)w * 0.12f;
  float x1 = (float)w * 0.88f;
  float cy = (float)h * 0.26f;
  float amp = fminf((float)h * 0.16f, 120.0f);
  float energy = analysis_energy(analysis, clamp_bar_count(cfg->bar_count));
  SDL_Color col = bar_color(cfg, 0.5f);
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 225);
  for (int i = 1; i < points; i++) {
    float a = (float)(i - 1) / (float)(points - 1);
    float b = (float)i / (float)(points - 1);
    float ya = cy + sinf(a * 18.0f + time_sec * 3.0f) * amp * (0.35f + energy);
    float yb = cy + sinf(b * 18.0f + time_sec * 3.0f) * amp * (0.35f + energy);
    SDL_RenderDrawLineF(renderer, x0 + (x1 - x0) * a, ya, x0 + (x1 - x0) * b, yb);
  }
}

static void draw_radial_graph_demo(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                                   const VisAnalysis *analysis, float time_sec) {
  int nodes = clamp_bar_count(cfg->bar_count);
  float cx = (float)w * 0.5f;
  float cy = (float)h * 0.5f;
  float base = fminf((float)w, (float)h) * 0.22f;
  float amp = fminf((float)w, (float)h) * 0.24f;
  float prev_x = 0.0f, prev_y = 0.0f, first_x = 0.0f, first_y = 0.0f;
  for (int i = 0; i < nodes; i++) {
    int bi = (i * cfg->bar_count) / nodes;
    float level = analysis->bars[bi];
    float ang = (float)(2.0 * M_PI * i / nodes) + time_sec * 0.18f;
    float r = base + amp * level;
    float x = cx + cosf(ang) * r;
    float y = cy + sinf(ang) * r;
    SDL_Color col = bar_color(cfg, (float)i / (float)(nodes - 1));
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 210);
    if (i > 0) {
      SDL_RenderDrawLineF(renderer, prev_x, prev_y, x, y);
    } else {
      first_x = x;
      first_y = y;
    }
    if (level > 0.08f) {
      fill_circle(renderer, x, y, 2.5f + level * 8.0f, col);
    }
    prev_x = x;
    prev_y = y;
  }
  SDL_Color end = bar_color(cfg, 0.9f);
  end.a = 210;
  SDL_SetRenderDrawColor(renderer, end.r, end.g, end.b, end.a);
  SDL_RenderDrawLineF(renderer, prev_x, prev_y, first_x, first_y);
}

static void draw_particle_field_demo(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                                     const VisAnalysis *analysis, float time_sec) {
  int count = clamp_bar_count(cfg->bar_count);
  float energy = analysis_energy(analysis, clamp_bar_count(cfg->bar_count));
  float peak = 0.0f, freq_t = 0.0f;
  peak_features(analysis, clamp_bar_count(cfg->bar_count), &peak, &freq_t);
  float cx = (float)w * (0.25f + freq_t * 0.5f);
  float cy = (float)h * 0.5f;
  for (int i = 0; i < count; i++) {
    int bi = (i * cfg->bar_count) / count;
    float level = analysis->bars[bi];
    float seed = (float)i * 12.9898f;
    float a = seed + time_sec * (0.25f + energy * 1.8f);
    float lane = (float)(i % 17) / 17.0f;
    float r = fminf((float)w, (float)h) * (0.08f + lane * 0.55f) * (0.55f + peak);
    float x = cx + cosf(a) * r + sinf(time_sec + seed) * level * 90.0f;
    float y = cy + sinf(a * 0.83f) * r + cosf(time_sec * 1.3f + seed) * level * 90.0f;
    SDL_Color col = bar_color(cfg, lane);
    col.a = (Uint8)(70.0f + fminf(level * 260.0f, 180.0f));
    fill_circle(renderer, x, y, 1.5f + level * 7.0f, col);
  }
}

static void draw_binary_tree_branch(SDL_Renderer *renderer, const VisConfig *cfg,
                                    const VisAnalysis *analysis, int depth, int max_depth,
                                    float x, float y, float len, float angle, int index,
                                    float growth) {
  if (depth > max_depth || len < 3.0f) {
    return;
  }
  int bi = (index * cfg->bar_count / 63) % cfg->bar_count;
  float level = analysis->bars[bi];
  float x2 = x + cosf(angle) * len;
  float y2 = y + sinf(angle) * len;
  SDL_Color col = bar_color(cfg, (float)depth / (float)max_depth);
  col.a = (Uint8)(120.0f + level * 100.0f);
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  SDL_RenderDrawLineF(renderer, x, y, x2, y2);
  float spread = 0.32f + level * 0.50f + growth * 0.20f;
  float next_len = len * (0.64f + level * 0.08f + growth * 0.04f);
  draw_binary_tree_branch(renderer, cfg, analysis, depth + 1, max_depth, x2, y2, next_len,
                          angle - spread, index * 2 + 1, growth);
  draw_binary_tree_branch(renderer, cfg, analysis, depth + 1, max_depth, x2, y2, next_len,
                          angle + spread, index * 2 + 2, growth);
}

static void draw_binary_tree_demo(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                                  const VisAnalysis *analysis, float time_sec) {
  static float growth = 0.0f;
  static float last_time = 0.0f;
  float dt = time_sec - last_time;
  if (dt < 0.0f || dt > 0.25f) {
    dt = 0.016f;
  }
  last_time = time_sec;
  float energy = analysis_energy(analysis, clamp_bar_count(cfg->bar_count));
  growth += (energy - growth) * fminf(dt * 8.0f, 1.0f);
  int depth = 7 + (int)(growth * 4.0f);
  float len = fminf((float)w, (float)h) * (0.15f + growth * 0.13f);
  draw_binary_tree_branch(renderer, cfg, analysis, 0, depth, (float)w * 0.5f, (float)h * 0.86f,
                          len, -(float)M_PI * 0.5f, 0, growth);
}

static void draw_demo(SDL_Renderer *renderer, int w, int h, const VisConfig *cfg,
                      const VisAnalysis *analysis, float time_sec) {
  switch (cfg->demo_mode) {
  case VIS_DEMO_SORTING:
    (void)analysis;
    draw_sorting_demo(renderer, w, h, cfg, time_sec);
    break;
  case VIS_DEMO_OFF:
  default:
    break;
  }
}

int vis_draw(SDL_Renderer *renderer, int width, int height, const VisConfig *cfg,
             const VisAnalysis *analysis, float time_sec, float dt_sec) {
  clear_frame(renderer, cfg);
  draw_animated_bg(renderer, width, height, cfg, analysis, time_sec);

  int drawn = clamp_bar_count(cfg->bar_count);
  if (cfg->demo_mode != VIS_DEMO_OFF) {
    draw_demo(renderer, width, height, cfg, analysis, time_sec);
    draw_reactive_asset(renderer, width, height, cfg, analysis, time_sec);
    return 0;
  }

  switch (cfg->layout) {
  case VIS_LAYOUT_HORIZONTAL:
    draw_horizontal(renderer, width, height, cfg, analysis);
    drawn = clamp_bar_count(cfg->bar_count);
    break;
  case VIS_LAYOUT_CIRCULAR:
    draw_circular(renderer, width, height, cfg, analysis);
    drawn = clamp_bar_count(cfg->bar_count);
    break;
  case VIS_LAYOUT_WAVE_PATH:
    draw_wave_path_demo(renderer, width, height, cfg, analysis, time_sec);
    drawn = 1;
    break;
  case VIS_LAYOUT_RADIAL_GRAPH:
    draw_radial_graph_demo(renderer, width, height, cfg, analysis, time_sec);
    drawn = clamp_bar_count(cfg->bar_count);
    break;
  case VIS_LAYOUT_PARTICLES:
    draw_particle_field_demo(renderer, width, height, cfg, analysis, time_sec);
    drawn = clamp_bar_count(cfg->bar_count);
    break;
  case VIS_LAYOUT_BINARY_TREE:
    draw_binary_tree_demo(renderer, width, height, cfg, analysis, time_sec);
    drawn = clamp_bar_count(cfg->bar_count);
    break;
  case VIS_LAYOUT_VERTICAL:
  default:
    drawn = draw_vertical(renderer, width, height, cfg, analysis, time_sec, dt_sec);
    break;
  }
  draw_reactive_asset(renderer, width, height, cfg, analysis, time_sec);
  return drawn;
}
