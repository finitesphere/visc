#ifndef VISC_VISUALIZER_H
#define VISC_VISUALIZER_H

#include "config.h"
#include "fft.h"

struct SDL_Renderer;

int vis_draw(struct SDL_Renderer *renderer, int width, int height, const VisConfig *cfg,
             const VisAnalysis *analysis, float time_sec, float dt_sec);

#endif
