#ifndef VISC_IMAGE_LOADER_H
#define VISC_IMAGE_LOADER_H

struct SDL_Surface;

#ifdef __cplusplus
extern "C" {
#endif

struct SDL_Surface *vis_image_load_surface(const char *path);

#ifdef __cplusplus
}
#endif

#endif
