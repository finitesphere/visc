#include "app.h"

#include <SDL.h>
#include <stdio.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  VisApp *app = vis_app_create();
  if (!app) {
    fprintf(stderr, "Out of memory\n");
    return 1;
  }
  if (!vis_app_init(app)) {
    vis_app_destroy(app);
    return 1;
  }
  vis_app_run(app);
  vis_app_destroy(app);
  return 0;
}
