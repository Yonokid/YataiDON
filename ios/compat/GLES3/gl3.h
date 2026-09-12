#pragma once
// raylib uses Khronos include paths; Apple's SDK exposes these via a framework.
#include <OpenGLES/ES3/gl.h>
#include <SDL3/SDL_video.h>

// UIKit renders into an SDL-owned framebuffer, not framebuffer zero. raylib
// returns to zero after render-texture allocation/drawing/readback. Translate
// those binds here so every offscreen path restores the actual window surface.
// Query the current window each time because rotation can recreate its buffers.
static inline void yataidon_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (framebuffer == 0) {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        if (window) {
            framebuffer = (GLuint)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0);
        }
    }
    glBindFramebuffer(target, framebuffer);
}
#define glBindFramebuffer yataidon_glBindFramebuffer

// SDL's UIKit swap operation also expects its view renderbuffer to stay bound.
// raylib unbinds renderbuffers after allocating depth attachments for 3D scenes.
static inline void yataidon_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    if (renderbuffer == 0) {
        SDL_Window* window = SDL_GL_GetCurrentWindow();
        if (window) {
            renderbuffer = (GLuint)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                SDL_PROP_WINDOW_UIKIT_OPENGL_RENDERBUFFER_NUMBER, 0);
        }
    }
    glBindRenderbuffer(target, renderbuffer);
}
#define glBindRenderbuffer yataidon_glBindRenderbuffer
