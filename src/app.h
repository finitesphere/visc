#ifndef VISC_APP_H
#define VISC_APP_H

#include "config.h"

#include <stdbool.h>

typedef struct VisApp VisApp;

VisApp *vis_app_create(void);
void vis_app_destroy(VisApp *app);
bool vis_app_init(VisApp *app);
void vis_app_run(VisApp *app);

#endif
