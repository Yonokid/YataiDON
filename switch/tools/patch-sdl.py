"""Build the pinned SDL2 Switch backend without a competing OpenGL driver."""
from pathlib import Path
import hashlib
import re
import sys

EXPECTED = {
    "CMakeLists.txt": "1172ed6270261a50579f3ac8d4016b3df50cd6c1f508cad61c6596813026d248",
    "src/video/switch/SDL_switchvideo.h": "cf3f5dd95289842a80b6591ecb390de5231072df1d1ceac406cba3e9ab244b8e",
    "src/video/switch/SDL_switchvideo.c": "2c40b97ebe4803efadc23730a1ad48a10db4662093bebd21a4c0a43f7db36400",
    "src/video/switch/SDL_switchswkb.c": "e4f5c514ddde2e2fb86b44666067ec4be72a5cb669ee8ab983b7f4591e310246",
}


def replace_function(text, name, body):
    pattern = r"(" + name + r"\([^\n]*\)\n\{).*?\n\}"
    result, count = re.subn(pattern, lambda match: match[1] + "\n" + body + "\n}", text, flags=re.DOTALL)
    if count != 1:
        raise RuntimeError(f"Unexpected SDL function: {name}")
    return result


def adapt(name, text):
    marker = "/* YataiDON adaptation: bgfx owns the libnx Vulkan surface. */\n"
    if name.endswith("SDL_switchswkb.c"):
        return marker + text[:text.index('#include "../../SDL_internal.h"')] + '''#include "../../SDL_internal.h"
#if SDL_VIDEO_DRIVER_SWITCH
#include "SDL_switchswkb.h"
extern void yataidon_switch_show_keyboard(void);
extern int yataidon_switch_keyboard_shown(void);
extern void yataidon_switch_cancel_keyboard_result(void);
void SWITCH_InitSwkb(void) {}
void SWITCH_PollSwkb(void) {}
void SWITCH_QuitSwkb(void) {}
SDL_bool SWITCH_HasScreenKeyboardSupport(_THIS) { return SDL_TRUE; }
SDL_bool SWITCH_IsScreenKeyboardShown(_THIS, SDL_Window *window) {
    return yataidon_switch_keyboard_shown() ? SDL_TRUE : SDL_FALSE;
}
void SWITCH_StartTextInput(_THIS) { yataidon_switch_show_keyboard(); }
void SWITCH_StopTextInput(_THIS) { yataidon_switch_cancel_keyboard_result(); }
#endif
'''
    if name == "CMakeLists.txt":
        text = text.replace('dep_option(SDL_VIRTUAL_JOYSTICK    "Enable the virtual-joystick driver" ON SDL_HIDAPI OFF)',
                            'option(SDL_VIRTUAL_JOYSTICK "Enable the virtual-joystick driver" ON)')
        start = text.index("elseif(NINTENDO_SWITCH)")
        end = text.index("\nendif()", start)
        block = text[start:end]
        block = block.replace("set(SDL_JOYSTICK_SWITCH 1)", "set(SDL_JOYSTICK_SWITCH 0)")
        block = re.sub(r"^    set\((SDL_VIDEO_OPENGL.*|HAVE_VIDEO_OPENGL.*|HAVE_VIDEO_OPENGLES.*|SDL_VIDEO_RENDER_OGL_ES2.*)\n", "", block, flags=re.MULTILINE)
        block = block.replace('    set(SOURCE_FILES ${SOURCE_FILES} ${SWITCH_VIDEO_SOURCES})',
                              '    list(FILTER SWITCH_VIDEO_SOURCES EXCLUDE REGEX "SDL_switchopengles\\\\.c$")\n    set(SOURCE_FILES ${SOURCE_FILES} ${SWITCH_VIDEO_SOURCES})')
        block = block.replace("EXTRA_LIBS EGL glapi drm_nouveau nx m", "EXTRA_LIBS nx m")
        block = block.replace("  add_definitions(-DSDL_VIDEO_STATIC_ANGLE)\n", "")
        return text[:start] + block + text[end:]
    if name.endswith(".h"):
        return marker + text.replace('#include "SDL_egl.h"', '').replace('EGLSurface egl_surface;', 'void *reserved;')
    text = marker + text.replace('#include "SDL_switchopengles.h"', '')
    text = re.sub(r"^    device->GL_.*\n", "", text, flags=re.MULTILINE)
    text = re.sub(r"    if\(_this->gl_config.driver_loaded > 0\) \{.*?\n    \}\n", "", text, flags=re.DOTALL)
    text = replace_function(text, "SWITCH_SetDisplayMode", "    return 0;")
    text = replace_function(text, "SWITCH_CreateWindow", '''    if (switch_window != NULL) {
        return SDL_SetError("Switch only supports one window");
    }
    Result rc = nwindowSetDimensions(nwindowGetDefault(), window->w, window->h);
    if (R_FAILED(rc)) {
        return SDL_SetError("Could not set NWindow dimensions: 0x%x", rc);
    }
    switch_window = window;
    operationMode = appletGetOperationMode();
    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
    return 0;''')
    text = replace_function(text, "SWITCH_DestroyWindow", '''    if (window == switch_window) {
        switch_window = NULL;
    }''')
    text = replace_function(text, "SWITCH_SetWindowSize", '''    nwindowSetDimensions(nwindowGetDefault(), window->w, window->h);''')
    return text


def main():
    root = Path(sys.argv[1])
    changes = []
    for name, checksum in EXPECTED.items():
        path = root / name
        original = path.with_suffix(path.suffix + ".yataidon-original")
        source = original.read_bytes() if original.exists() else path.read_bytes()
        if hashlib.sha256(source).hexdigest() != checksum:
            raise RuntimeError(f"Unexpected SDL revision: {path}")
        patched = adapt(name, source.decode()).encode()
        if path.read_bytes() not in (source, patched):
            raise RuntimeError(f"Preserving unexpected SDL edits: {path}")
        changes.append((path, original, source, patched))
    for path, original, source, patched in changes:
        if not original.exists():
            original.write_bytes(source)
        if path.read_bytes() != patched:
            path.write_bytes(patched)


if __name__ == "__main__":
    main()
