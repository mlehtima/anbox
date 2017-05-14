#ifndef SFDROID_RENDERER_H
#define SFDROID_RENDERER_H

#include "anbox/graphics/renderer.h"

#include <wayland-client.h>
#include <wayland-egl.h>

#include <dlfcn.h>

#include <map>

#define HWC_LAYER_NAME_MAX_LENGTH 128

struct buffer_info_t {
    char layer_name[HWC_LAYER_NAME_MAX_LENGTH];
    int width;
    int height;
    int stride;
    int format;
};

typedef struct native_handle
{
    int version;        /* sizeof(native_handle_t) */
    int numFds;         /* number of file-descriptors at &data[0] */
    int numInts;        /* number of ints at &data[numFds] */
    int data[0];        /* numFds + numInts ints */
} native_handle_t;

struct RendererWindow;

class Renderer : public anbox::graphics::Renderer {
private:
  void *lhcommon;
  void *(*hybris_egl_display_get_mapping)(EGLNativeDisplayType dpy);
public:
  RendererWindow* createNativeWindow(EGLNativeWindowType native_window, void *surface, EGLNativeDisplayType egl_dpy);
  void destroyNativeWindow(RendererWindow* window);
  void destroyNativeWindow(EGLNativeWindowType native_window);
  bool initialize(EGLNativeDisplayType nativeDisplay);
  void finalize();
  bool draw(EGLNativeWindowType native_window,
            const anbox::graphics::Rect& window_frame,
            const RenderableList& renderables) override;

  std::map<EGLNativeWindowType, RendererWindow*> m_nativeWindows;
  EGLNativeDisplayType egl_dpy;
};

#endif

