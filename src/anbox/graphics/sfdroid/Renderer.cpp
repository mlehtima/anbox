#include "anbox/graphics/sfdroid/Renderer.h"

#include <EGL/egl.h>

#include "wayland-android-client-protocol.h"

#include <iostream>

using namespace std;

struct RendererWindow {
  EGLNativeWindowType native_window = 0;
  wl_surface *surface = 0;
  EGLNativeDisplayType egl_dpy = 0;
};

RendererWindow *Renderer::createNativeWindow(EGLNativeWindowType native_window, void *surface, EGLNativeDisplayType egl_dpy) {
  RendererWindow *win = new RendererWindow;
  win->native_window = native_window;
  win->surface = (struct wl_surface*)surface;
  win->egl_dpy = egl_dpy;
  m_nativeWindows.insert({native_window, win});
  return win;
}

void Renderer::destroyNativeWindow(EGLNativeWindowType native_window) {
  map<EGLNativeWindowType, RendererWindow*>::iterator win;
  if((win = m_nativeWindows.find(native_window)) != m_nativeWindows.end()) {
    delete win->second;
    m_nativeWindows.erase(win);
  }
}

bool Renderer::initialize(EGLNativeDisplayType nativeDisplay) {
    egl_dpy = nativeDisplay;
    // hack
    lhcommon = dlopen("/usr/lib/libEGL.so.1", RTLD_LAZY);
    hybris_egl_display_get_mapping = (void* (*)(EGLNativeDisplayType))dlsym(lhcommon, "hybris_egl_display_get_mapping");
    return true;
}

void Renderer::finalize() {
    dlclose(lhcommon);
}
#include <string.h>
#include <stdlib.h>
#define ANDROID_NATIVE_MAKE_CONSTANT(a,b,c,d) \
    (((unsigned)(a)<<24)|((unsigned)(b)<<16)|((unsigned)(c)<<8)|(unsigned)(d))
#define ANDROID_NATIVE_WINDOW_MAGIC \
    ANDROID_NATIVE_MAKE_CONSTANT('_','w','n','d')
#define ANDROID_NATIVE_BUFFER_MAGIC \
    ANDROID_NATIVE_MAKE_CONSTANT('_','b','f','r')

typedef struct android_native_base_t
{
    /* a magic value defined by the actual EGL native type */
    int magic;

    /* the sizeof() of the actual EGL native type */
    int version;

    void* reserved[4];

    /* reference-counting interface */
    void (*incRef)(struct android_native_base_t* base);
    void (*decRef)(struct android_native_base_t* base);
} android_native_base_t;

typedef struct ANativeWindowBuffer
{
#ifdef __cplusplus
    ANativeWindowBuffer() {
        common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
        common.version = sizeof(ANativeWindowBuffer);
        memset(common.reserved, 0, sizeof(common.reserved));
    }

    // Implement the methods that sp<ANativeWindowBuffer> expects so that it
    // can be used to automatically refcount ANativeWindowBuffer's.
    void incStrong(const void* /*id*/) const {
        common.incRef(const_cast<android_native_base_t*>(&common));
    }
    void decStrong(const void* /*id*/) const {
        common.decRef(const_cast<android_native_base_t*>(&common));
    }
#endif

    struct android_native_base_t common;

    int width;
    int height;
    int stride;
    int format;
    int usage;

    void* reserved[2];

    native_handle_t *handle;

    void* reserved_proc[8];
} ANativeWindowBuffer_t;

void dummy_f(struct android_native_base_t* base)
{
}

struct WaylandDisplay {
        void *base;

        wl_display *wl_dpy;
        wl_event_queue *queue;
        wl_registry *registry;
        android_wlegl *wlegl;
};

bool Renderer::draw(EGLNativeWindowType native_window,
                    const anbox::graphics::Rect &window_frame,
                    const RenderableList &renderables) {
#if 0
    float xf = 1.f;
    float yf = 1.f;
    float texcoords[] = {
        0.f, 0.f,
        xf, 0.f,
        0.f, yf,
        xf, yf,
    };

    float vtxcoords[] = {
        0.f, 0.f,
        (float)window_frame->width, 0.f,
        0.f, (float)window_frame->height,
        (float)window_frame->width, (float)window_frame->height,
    };

    glBindTexture(GL_TEXTURE_2D, tex);

    glVertexPointer(2, GL_FLOAT, 0, &vtxcoords);
    glTexCoordPointer(2, GL_FLOAT, 0, &texcoords);

    EGLImage egl_img = eglCreateImageKHR(egl_dpy, eglGetCurrentContext(), EGL_NATIVE_BUFFER_ANDROID, renderables[0].buffer(), NULL);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_img);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(egl_dpy, ..);

    eglDestroyImageKHR(egl_dpy, egl_img);
    delete buffer;
#endif
#if 0 // renders garbage...
    // only for testing purposes
    struct ANativeWindowBuffer *buffer = new ANativeWindowBuffer;
    buffer->height = 1280;
    buffer->width = 720;
    buffer->stride = 720;
    buffer->format = 1;
    buffer->usage = 0;
    buffer->handle = (native_handle_t*)renderables[0].buffer();
    buffer->common.incRef = dummy_f;
    buffer->common.decRef = dummy_f;
    std::cout << " aaa " << renderables[0].buffer() << std::endl;
    static int (*pfn_eglHybrisWaylandPostBuffer)(EGLNativeWindowType, void*) = (int (*)(EGLNativeWindowType, void *))eglGetProcAddress("eglHybrisWaylandPostBuffer");
    pfn_eglHybrisWaylandPostBuffer(native_window, buffer);
#endif

    struct wl_surface *w_surface = NULL;

    std::map<EGLNativeWindowType, RendererWindow*>::iterator it;
    if((it = m_nativeWindows.find(native_window)) != m_nativeWindows.end()) {
        w_surface = it->second->surface;
    } else {
        std::cerr << "surface not found" << std::endl;
        return 1;
    }

    int width = renderables[0].width();
    int height = renderables[0].height();
    int stride = renderables[0].stride();
    int format = renderables[0].format();

    WaylandDisplay *w_dpy = reinterpret_cast<WaylandDisplay*>(hybris_egl_display_get_mapping(it->second->egl_dpy));

    native_handle_t *buffer = reinterpret_cast<native_handle_t*>(renderables[0].buffer());

    struct wl_buffer *w_buffer;
    struct wl_array ints;
    int *the_ints;
    struct android_wlegl_handle *wlegl_handle;

    wl_array_init(&ints);
    the_ints = (int*)wl_array_add(&ints, buffer->numInts * sizeof(int));
    memcpy(the_ints, buffer->data + buffer->numFds, buffer->numInts * sizeof(int));

    wlegl_handle = android_wlegl_create_handle(w_dpy->wlegl, buffer->numFds, &ints);
    wl_array_release(&ints);

    for (int i = 0; i < buffer->numFds; i++)
    {
        android_wlegl_handle_add_fd(wlegl_handle, buffer->data[i]);
    }

    w_buffer = android_wlegl_create_buffer(w_dpy->wlegl, width, height, stride, format, 0x00000200U/*GRALLOC_USAGE_HW_RENDER*/, wlegl_handle);
    android_wlegl_handle_destroy(wlegl_handle);

//    wl_buffer_add_listener(w_buffer, &w_buffer_listener, this);
/*
    int ret = 0;
    while(frame_callback_ptr && ret != -1)
    {
        ret = wl_display_dispatch(wayland_helper::display);
    }
*/

    /*frame_callback_ptr = wl_surface_frame(w_surface);
    wl_callback_add_listener(frame_callback_ptr, &w_frame_listener, this);*/

    wl_surface_attach(w_surface, w_buffer, 0, 0);
    wl_surface_damage(w_surface, 0, 0, width, height);
    wl_surface_commit(w_surface);

    wl_display_flush(w_dpy->wl_dpy);

    wl_buffer_destroy(w_buffer);

    return 0;


}

