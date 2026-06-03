#include "visualizer.h"

#include <math.h>
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
static float g_beat_flash = 0.0f;

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
  case VIS_BG_BEAT_FLASH: {
    if (energy > 0.45f) {
      g_beat_flash = 1.0f;
    }
    g_beat_flash -= 0.04f * speed;
    if (g_beat_flash < 0.0f) {
      g_beat_flash = 0.0f;
    }
    memcpy(bg, cfg->color_bg, sizeof(bg));
    float flash = g_beat_flash * g_beat_flash;
    bg[0] += cfg->color_bar2[0] * flash * 0.6f;
    bg[1] += cfg->color_bar2[1] * flash * 0.6f;
    bg[2] += cfg->color_bar2[2] * flash * 0.6f;
    bg[3] = cfg->transparent ? 0.15f + 0.35f * flash : 1.0f;
    draw_solid_bg(renderer, bg);
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
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, alpha);
    SDL_FRect puff = {x + drift - radius * 0.5f, y - radius * 0.5f, radius, radius};
    SDL_RenderFillRectF(renderer, &puff);
    SDL_FRect puff2 = {x + drift + bw * 0.15f, y - radius * 0.35f, radius * 0.85f, radius * 0.85f};
    SDL_RenderFillRectF(renderer, &puff2);
  }
}

static void fill_bar(SDL_Renderer *renderer, const VisConfig *cfg, SDL_FRect rect, SDL_Color col) {
  if (cfg->bar_glow && rect.h > 2.0f) {
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, (Uint8)(col.a / 3));
    SDL_FRect glow = {rect.x - 2.0f, rect.y - 2.0f, rect.w + 4.0f, rect.h + 4.0f};
    SDL_RenderFillRectF(renderer, &glow);
  }
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  SDL_RenderFillRectF(renderer, &rect);
  if (cfg->rounded_bars && rect.h > 6.0f) {
    SDL_FRect cap = {rect.x, rect.y, rect.w, 5.0f};
    SDL_RenderFillRectF(renderer, &cap);
  }
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
  int bars = cfg->bar_count;
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
  int bars = cfg->bar_count;
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

int vis_draw(SDL_Renderer *renderer, int width, int height, const VisConfig *cfg,
             const VisAnalysis *analysis, float time_sec, float dt_sec) {
  clear_frame(renderer, cfg);
  draw_animated_bg(renderer, width, height, cfg, analysis, time_sec);

  int drawn = clamp_bar_count(cfg->bar_count);
  switch (cfg->layout) {
  case VIS_LAYOUT_HORIZONTAL:
    draw_horizontal(renderer, width, height, cfg, analysis);
    drawn = bars_that_fit(width, cfg, NULL, NULL);
    break;
  case VIS_LAYOUT_CIRCULAR:
    draw_circular(renderer, width, height, cfg, analysis);
    drawn = clamp_bar_count(cfg->bar_count);
    break;
  case VIS_LAYOUT_VERTICAL:
  default:
    drawn = draw_vertical(renderer, width, height, cfg, analysis, time_sec, dt_sec);
    break;
  }
  return drawn;
}
