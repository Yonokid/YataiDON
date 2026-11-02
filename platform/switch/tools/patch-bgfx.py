"""Adapt pinned bgfx to mesa-switch's static Vulkan and libnx NWindow.

Only pristine pinned sources or this script's exact output are accepted.
"""
import hashlib
from pathlib import Path
import sys

EXPECTED = {
    "bx/src/os.cpp": "133242ad8856ae6e2190e45018ab548e92c4421e44a06b3c633a427a4c4acdb6",
    "bx/include/bx/platform.h": "5da3c3659dcabfb660efc6997336f3924743ebf8912a2e14a473b3e54098cdee",
    "cmake/bx/bx.cmake": "79193bce5be24cfe443b7d10e74146eaa4da2306785a6d3676c0a63b2ebd6778",
    "bgfx/src/renderer_vk.cpp": "1c7a7f737ef61d814cd3db86022385740c391bcef087e86d7c6d9c8ee362cf05",
    "cmake/bgfx/bgfx.cmake": "91898ae3664b34b11b12f2b60da341665c8649559300810b0b05f46fd14b8bfe",
}


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Unexpected bgfx source anchor: {old!r}")
    return text.replace(old, new, 1)


def adapt(name, text):
    if name == "bx/src/os.cpp":
        return text.replace("!BX_PLATFORM_PS4 && !BX_PLATFORM_PS5", "!BX_PLATFORM_PS4 && !BX_PLATFORM_PS5 && !BX_PLATFORM_NX")
    if name.endswith("platform.h"):
        return replace_once(text, "#elif defined(__has_builtin) && __has_builtin(__is_target_os) && __is_target_os(xros)",
                            "#elif defined(__SWITCH__)\n#\tundef BX_PLATFORM_NX\n#\tdefine BX_PLATFORM_NX 1\n#elif defined(__has_builtin) && __has_builtin(__is_target_os) && __is_target_os(xros)")
    if name.endswith("bx.cmake"):
        return text.replace("elseif(UNIX)", "elseif(UNIX AND NOT SWITCH)")
    if name.endswith("bgfx.cmake"):
        return text.replace("if(UNIX\n", "if(UNIX AND NOT SWITCH\n")
    text = replace_once(text, '#\tinclude "video_vk.h"', '''#\tinclude "video_vk.h"
#if BX_PLATFORM_NX
#include <switch.h>
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance, const char*);
extern "C" PFN_vkVoidFunction vk_common_GetDeviceProcAddr(VkDevice, const char*);
static void* yataidonSwitchLookup(void*, const char* name)
{
    if (0 == bx::strCmp(name, "vkGetInstanceProcAddr"))
        return (void*)vk_icdGetInstanceProcAddr;
    if (0 == bx::strCmp(name, "vkGetDeviceProcAddr"))
        return (void*)vk_common_GetDeviceProcAddr;
    return (void*)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, name);
}
#endif''')
    start = text.index('\t\t\tm_vulkan1Dll = bx::dlopen(')
    end = text.index('\n', text.index('\t\t\t\t);', start))
    block = text[start:end]
    text = text[:start] + '#if BX_PLATFORM_NX\n\t\t\tm_vulkan1Dll = (void*)1;\n#else\n' + block + '\n#endif' + text[end:]
    text = replace_once(text, 'bx::dlsym(m_vulkan1Dll, #_func)', 'yataidonVulkanLookup(m_vulkan1Dll, #_func)')
    text = text.replace('bx::dlclose(m_vulkan1Dll);', 'yataidonVulkanClose(m_vulkan1Dll);')
    # The shared SDL2 gfx path supplies an SDL_Window; Mesa requires an NWindow.
    text = replace_once(text, '#elif BX_PLATFORM_NX\n\t\tif (NULL != vkCreateViSurfaceNN)',
                        '#elif BX_PLATFORM_NX\n\t\t_nwh = nwindowGetDefault();\n\t\tif (NULL != vkCreateViSurfaceNN)')
    helpers = '''
static void* yataidonVulkanLookup(void* library, const char* name)
{
#if BX_PLATFORM_NX
    return yataidonSwitchLookup(library, name);
#else
    return bx::dlsym(library, name);
#endif
}
static void yataidonVulkanClose(void* library)
{
#if BX_PLATFORM_NX
    BX_UNUSED(library);
#else
    bx::dlclose(library);
#endif
}
'''
    text = text.replace('\nnamespace bgfx { namespace vk', helpers + '\nnamespace bgfx { namespace vk', 1)
    return text


def main():
    root = Path(sys.argv[1]).resolve()
    changes = []
    for name, checksum in EXPECTED.items():
        path = root / name
        original = path.with_suffix(path.suffix + ".yataidon-original")
        source = original.read_bytes() if original.exists() else path.read_bytes()
        if hashlib.sha256(source).hexdigest() != checksum:
            raise RuntimeError(f"Unexpected pinned bgfx source: {path}")
        patched = adapt(name, source.decode()).encode()
        if path.read_bytes() not in (source, patched):
            raise RuntimeError(f"Preserving unexpected local edits: {path}")
        changes.append((path, original, source, patched))
    for path, original, source, patched in changes:
        if not original.exists():
            original.write_bytes(source)
        if path.read_bytes() != patched:
            path.write_bytes(patched)


if __name__ == "__main__":
    main()
