cmake_minimum_required(VERSION 3.16)
project(visc C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
file(MAKE_DIRECTORY ${VENDOR_DIR})

function(visc_vendor_download name url)
  set(dest ${VENDOR_DIR}/${name})
  if(NOT EXISTS ${dest})
    message(STATUS "Downloading vendor/${name}")
    file(DOWNLOAD ${url} ${dest} SHOW_PROGRESS TLS_VERIFY ON)
  endif()
endfunction()

visc_vendor_download(minimp3.h https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h)
visc_vendor_download(kiss_fft.h https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.h)
visc_vendor_download(kiss_fft_log.h https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft_log.h)
visc_vendor_download(kiss_fft.c https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.c)
visc_vendor_download(_kiss_fft_guts.h https://raw.githubusercontent.com/mborgerding/kissfft/master/_kiss_fft_guts.h)
visc_vendor_download(kiss_fftr.h https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fftr.h)
visc_vendor_download(kiss_fftr.c https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fftr.c)
visc_vendor_download(tinyfiledialogs.h https://raw.githubusercontent.com/native-toolkit/libtinyfiledialogs/master/tinyfiledialogs.h)
visc_vendor_download(tinyfiledialogs.c https://raw.githubusercontent.com/native-toolkit/libtinyfiledialogs/master/tinyfiledialogs.c)

if(MSVC)
  add_compile_options(/W4)
  add_compile_definitions(_USE_MATH_DEFINES)
else()
  add_compile_options(-Wall -Wextra)
endif()

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.10
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

set(PA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PA_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  portaudio
  GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
  GIT_TAG v19.7.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(portaudio)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(VISC_SOURCES
  src/main.c
  src/app.c
  src/audio.c
  src/fft.c
  src/visualizer.c
  src/config.c
  src/themes.c
  src/window.c
  src/stats.c
  src/ui.cpp
  src/audio_loopback.cpp
  vendor/kiss_fft.c
  vendor/kiss_fftr.c
  vendor/tinyfiledialogs.c
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

add_executable(visc ${VISC_SOURCES})

target_include_directories(visc PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(visc PRIVATE SDL_MAIN_HANDLED)

target_link_libraries(visc PRIVATE
  SDL2::SDL2-static
  portaudio_static
)

if(WIN32)
  target_link_libraries(visc PRIVATE winmm ole32 uuid setupapi version imm32 avrt)
  target_compile_definitions(visc PRIVATE VISC_HAS_LOOPBACK=1)
endif()

if(APPLE)
  target_link_libraries(visc PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation" "-framework CoreServices")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(visc PRIVATE pthread m dl)
endif()

install(TARGETS visc RUNTIME DESTINATION bin)
