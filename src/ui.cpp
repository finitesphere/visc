#include "ui.h"

#include "audio.h"
#include "fft.h"
#include "config.h"
#include "stats.h"
#include "themes.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <SDL.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "tinyfiledialogs.h"
}

static bool g_inited = false;
static int g_selected_theme = VIS_THEME_AURORA;
static bool g_theme_custom = false;

static void mark_theme_custom(void) { g_theme_custom = true; }

void vis_ui_set_theme(int theme_id) {
  if (theme_id >= 0 && theme_id < vis_theme_count()) {
    g_selected_theme = theme_id;
    g_theme_custom = false;
  } else {
    g_theme_custom = true;
  }
}

int vis_ui_theme_id(void) {
  return g_theme_custom ? VIS_THEME_CUSTOM : g_selected_theme;
}

void vis_ui_theme_next(VisConfig *cfg) {
  int next = g_theme_custom ? 0 : (g_selected_theme + 1) % vis_theme_count();
  g_selected_theme = next;
  g_theme_custom = false;
  vis_theme_apply(cfg, (VisThemeId)next);
}

void vis_ui_theme_prev(VisConfig *cfg) {
  int prev = g_theme_custom ? vis_theme_count() - 1
                            : (g_selected_theme - 1 + vis_theme_count()) % vis_theme_count();
  g_selected_theme = prev;
  g_theme_custom = false;
  vis_theme_apply(cfg, (VisThemeId)prev);
}

bool vis_ui_init(SDL_Window *window, SDL_Renderer *renderer) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.FrameRounding = 5.0f;
  style.GrabRounding = 4.0f;
  style.WindowBorderSize = 0.0f;

  if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
    return false;
  }
  if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
    return false;
  }
  g_inited = true;
  return true;
}

void vis_ui_shutdown(void) {
  if (!g_inited) {
    return;
  }
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  g_inited = false;
}

void vis_ui_begin_frame(void) {
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
}

static void color_edit4(const char *label, float c[4]) {
  ImGui::ColorEdit4(label, c, ImGuiColorEditFlags_Float);
}

void vis_ui_draw_stats_overlay(bool show) {
  if (!show) {
    return;
  }
  const VisStats *s = vis_stats_get();
  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 12.0f, io.DisplaySize.y - 12.0f),
                         ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.88f));
  if (ImGui::Begin("##visc_stats", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_NoFocusOnAppearing)) {
    ImGui::Text("visc stats");
    ImGui::Separator();
    ImGui::Text("Peak: %.0f Hz  %.1f dB", s->peak_hz, s->peak_db);
    ImGui::Text("Sample rate: %.0f Hz", s->sample_rate_hz);
    ImGui::Text("Latency (est.): %.1f ms", s->latency_ms);
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", s->fps);
    ImGui::Text("FFT: %.3f ms avg", s->fft_ms_avg);
    ImGui::Text("Min/Max: %.3f / %.3f ms", s->fft_ms_min, s->fft_ms_max);
    float bin_hz = s->sample_rate_hz / (float)VISC_FFT_SIZE;
    ImGui::Text("FFT res: %.2f Hz/bin", bin_hz);
    ImGui::Text("Bars: %d drawn / %d requested", s->bars_drawn, s->bars_requested);
  }
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

void vis_ui_draw(VisConfig *cfg, char *file_path, size_t file_path_size, bool *show_panel) {
  if (!*show_panel) {
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    if (ImGui::Begin("visc_hint", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {
      ImGui::Text("Tab: panel | I: stats | T: transparent | O: on-top | Esc: quit");
    }
    ImGui::End();
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(440, 720), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("visc settings", show_panel)) {
    ImGui::End();
    return;
  }

  /* Themes first — easy to find without scrolling past audio controls. */
  ImGui::Text("Themes");
  ImGui::TextDisabled("Click a preset or use [ and ] keys");
  {
    const float btn_w = 128.0f;
    const float btn_h = 26.0f;
    for (int i = 0; i < vis_theme_count(); i++) {
      if (i > 0 && (i % 3) != 0) {
        ImGui::SameLine();
      }
      bool active = !g_theme_custom && g_selected_theme == i;
      if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 0.90f, 1.0f));
      }
      if (ImGui::Button(vis_theme_name((VisThemeId)i), ImVec2(btn_w, btn_h))) {
        g_selected_theme = i;
        g_theme_custom = false;
        vis_theme_apply(cfg, (VisThemeId)i);
      }
      if (active) {
        ImGui::PopStyleColor(2);
      }
    }
  }
  const char *theme_preview =
      g_theme_custom ? "Custom (manual tweaks)" : vis_theme_name((VisThemeId)g_selected_theme);
  if (ImGui::BeginCombo("Theme (list)", theme_preview)) {
    for (int i = 0; i < vis_theme_count(); i++) {
      bool picked = !g_theme_custom && g_selected_theme == i;
      if (ImGui::Selectable(vis_theme_name((VisThemeId)i), picked)) {
        g_selected_theme = i;
        g_theme_custom = false;
        vis_theme_apply(cfg, (VisThemeId)i);
      }
      if (picked) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  ImGui::Text("Audio source");
  static VisAudioDevice devices[64];
  static int device_count = 0;
  static int selected_device = 0;
  static int source_kind = 1;
  static bool devices_dirty = true;

  if (devices_dirty) {
    if (source_kind == 0) {
      device_count = vis_audio_refresh_devices(devices, 64);
    } else if (source_kind == 1) {
      device_count = vis_audio_refresh_loopback_devices(devices, 64);
    }
    devices_dirty = false;
  }
  if (ImGui::Button("Refresh devices")) {
    devices_dirty = true;
  }

#ifdef VISC_HAS_LOOPBACK
  const char *source_labels[] = {"Microphone / line in", "Desktop audio (speakers)", "MP3 file"};
  const int source_kind_count = 3;
#else
  const char *source_labels[] = {"Microphone / line in", "MP3 file"};
  const int source_kind_count = 2;
#endif

  if (ImGui::Combo("Source", &source_kind, source_labels, source_kind_count)) {
    devices_dirty = true;
    selected_device = 0;
  }

  if (source_kind == 0) {
    if (device_count > 0) {
      if (selected_device >= device_count) {
        selected_device = 0;
      }
      if (ImGui::BeginCombo("Input device", devices[selected_device].name)) {
        for (int i = 0; i < device_count; i++) {
          bool selected = (selected_device == i);
          if (ImGui::Selectable(devices[i].name, selected)) {
            selected_device = i;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      if (ImGui::Button("Start capture")) {
        vis_audio_start_device(devices[selected_device].index);
      }
    } else {
      ImGui::TextDisabled("No input devices found");
    }
  }
#ifdef VISC_HAS_LOOPBACK
  else if (source_kind == 1) {
    ImGui::TextWrapped(
        "Captures what Windows plays on the selected output (e.g. your DAC). "
        "Audio must be playing on that device.");
    if (device_count > 0) {
      if (selected_device >= device_count) {
        selected_device = 0;
      }
      if (ImGui::BeginCombo("Playback device", devices[selected_device].name)) {
        for (int i = 0; i < device_count; i++) {
          bool selected = (selected_device == i);
          if (ImGui::Selectable(devices[i].name, selected)) {
            selected_device = i;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      if (ImGui::Button("Start desktop capture")) {
        vis_audio_start_loopback(devices[selected_device].index);
      }
    } else {
      ImGui::TextDisabled("No playback devices found");
    }
  }
#endif
  else {
    ImGui::InputText("MP3 path", file_path, file_path_size);
    if (ImGui::Button("Browse MP3...")) {
      char picked[512];
      if (vis_ui_pick_mp3_file(picked, sizeof(picked))) {
        snprintf(file_path, file_path_size, "%s", picked);
      }
    }
    if (ImGui::Button("Play file")) {
      if (file_path[0] != '\0') {
        vis_audio_start_file(file_path);
      }
    }
  }

  if (ImGui::Button("Stop audio")) {
    vis_audio_stop();
  }
  ImGui::TextWrapped("%s", vis_audio_current_label());

  ImGui::Separator();
  ImGui::Text("Window & fun");
  if (ImGui::Checkbox("Transparent overlay", &cfg->transparent)) {
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "See your desktop through empty areas (black = transparent on Windows).");
  }
  if (ImGui::Checkbox("Always on top", &cfg->always_on_top)) {
  }
  if (ImGui::Checkbox("Stats overlay (bottom-right)", &cfg->show_stats)) {
  }
  if (ImGui::Checkbox("Bar glow", &cfg->bar_glow)) {
  }
  if (ImGui::Checkbox("Party mode (auto themes)", &cfg->party_mode)) {
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Cycles themes every few seconds. Press P to toggle.");
  }

  int bg_anim = (int)cfg->bg_anim;
  const char *bg_names[] = {"Off",           "Gradient shift", "Audio pulse", "Starfield",
                            "Rainbow wash", "Beat flash"};
  if (ImGui::Combo("Animated background", &bg_anim, bg_names, VIS_BG_COUNT)) {
    cfg->bg_anim = (VisBgAnim)bg_anim;
  }
  ImGui::SliderFloat("FX speed", &cfg->bg_anim_speed, 0.25f, 3.0f, "%.2f");

  int bar_style = (int)cfg->bar_style;
  const char *style_names[] = {"Solid", "Fluid (water)", "Cloud (soft)"};
  if (ImGui::Combo("Bar style", &bar_style, style_names, VIS_BAR_STYLE_COUNT)) {
    cfg->bar_style = (VisBarStyle)bar_style;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Fluid and cloud use springy motion; best with vertical layout.");
  }

  ImGui::Separator();
  ImGui::Text("Display");
  if (g_theme_custom) {
    ImGui::TextDisabled("Using custom tweaks — pick a theme at the top to reset.");
  }

  if (ImGui::CollapsingHeader("Customize", ImGuiTreeNodeFlags_DefaultOpen)) {
    int layout = (int)cfg->layout;
    const char *layouts[] = {"Vertical", "Horizontal", "Circular"};
    if (ImGui::Combo("Layout", &layout, layouts, VIS_LAYOUT_COUNT)) {
      cfg->layout = (VisLayout)layout;
      if (ImGui::IsItemEdited()) {
        mark_theme_custom();
      }
    }

    int mode = (int)cfg->mode;
    const char *modes[] = {"Spectrum (FFT)", "Waveform"};
    if (ImGui::Combo("Mode", &mode, modes, VIS_MODE_COUNT)) {
      cfg->mode = (VisMode)mode;
      if (ImGui::IsItemEdited()) {
        mark_theme_custom();
      }
    }

    if (ImGui::SliderInt("Bar count", &cfg->bar_count, 8, 256) && ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Bar width (0=auto)", &cfg->bar_width_px, 0.0f, 48.0f, "%.1f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Bar gap", &cfg->bar_gap_px, 0.0f, 24.0f, "%.1f") && ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::Checkbox("Gradient bars", &cfg->use_gradient) && ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    color_edit4("Bar color A", cfg->color_bar);
    if (ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    color_edit4("Bar color B", cfg->color_bar2);
    if (ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    color_edit4("Background", cfg->color_bg);
    if (ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Sensitivity", &cfg->sensitivity, 0.1f, 5.0f, "%.2f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Min bar height", &cfg->min_bar_norm, 0.0f, 0.25f, "%.3f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Attack smooth", &cfg->smooth_attack, 0.05f, 1.0f, "%.2f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Decay smooth", &cfg->smooth_decay, 0.02f, 0.8f, "%.2f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Freq low (Hz)", &cfg->freq_low_hz, 20.0f, 500.0f, "%.0f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::SliderFloat("Freq high (Hz)", &cfg->freq_high_hz, 500.0f, 20000.0f, "%.0f") &&
        ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::Checkbox("Mirrored", &cfg->mirrored) && ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::Checkbox("Rounded tops", &cfg->rounded_bars) && ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    if (ImGui::Checkbox("Peak hold", &cfg->show_peaks) && ImGui::IsItemEdited()) {
      mark_theme_custom();
    }
    ImGui::SliderFloat("FPS cap", &cfg->fps_cap, 30.0f, 144.0f, "%.0f");
  }

  ImGui::Separator();
  if (ImGui::Button("Save settings...")) {
    char path[512];
    if (vis_ui_pick_config_save(path, sizeof(path))) {
      vis_config_save(cfg, path, vis_ui_theme_id());
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Load settings...")) {
    char path[512];
    if (vis_ui_pick_config_load(path, sizeof(path))) {
      int theme = VIS_THEME_AURORA;
      if (vis_config_load(cfg, path, &theme)) {
        vis_ui_set_theme(theme);
      }
    }
  }

  ImGui::End();
}

void vis_ui_render(SDL_Renderer *renderer) {
  ImGui::Render();
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void vis_ui_process_event(const SDL_Event *event) {
  ImGui_ImplSDL2_ProcessEvent(event);
}

bool vis_ui_want_capture_mouse(void) {
  return ImGui::GetIO().WantCaptureMouse;
}

bool vis_ui_want_capture_keyboard(void) {
  return ImGui::GetIO().WantCaptureKeyboard;
}

bool vis_ui_pick_mp3_file(char *out_path, size_t out_size) {
  const char *filters[] = {"*.mp3"};
  const char *sel = tinyfd_openFileDialog("Select MP3", "", 1, filters, "MP3 files", 0);
  if (!sel) {
    return false;
  }
  snprintf(out_path, out_size, "%s", sel);
  return true;
}

bool vis_ui_pick_config_save(char *out_path, size_t out_size) {
  const char *sel = tinyfd_saveFileDialog("Save visc settings", "visc.ini", 0, nullptr, nullptr);
  if (!sel) {
    return false;
  }
  snprintf(out_path, out_size, "%s", sel);
  return true;
}

bool vis_ui_pick_config_load(char *out_path, size_t out_size) {
  const char *sel = tinyfd_openFileDialog("Load visc settings", "", 0, nullptr, nullptr, 0);
  if (!sel) {
    return false;
  }
  snprintf(out_path, out_size, "%s", sel);
  return true;
}
