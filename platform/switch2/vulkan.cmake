# OunceVK installs libvulkan.a as a linker script that names its static
# implementation and runtime dependencies.
set(YATAIDON_MESA_LIBRARY "" CACHE FILEPATH "Optional OunceVK Vulkan link script or archive")
if(YATAIDON_MESA_LIBRARY)
  if(NOT EXISTS "${YATAIDON_MESA_LIBRARY}")
    message(FATAL_ERROR "YATAIDON_MESA_LIBRARY does not exist: ${YATAIDON_MESA_LIBRARY}")
  endif()
  set(_switch2_vulkan "${YATAIDON_MESA_LIBRARY}")
else()
  find_library(_switch2_vulkan NAMES vulkan PATHS "${SWITCH_MESA_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
endif()
find_path(_switch2_vulkan_headers NAMES vulkan/vulkan.h
  PATHS "${SWITCH_MESA_PREFIX}/include" NO_DEFAULT_PATH REQUIRED)
add_library(Vulkan::Vulkan INTERFACE IMPORTED)
set_target_properties(Vulkan::Vulkan PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_switch2_vulkan_headers}"
  INTERFACE_LINK_LIBRARIES "${_switch2_vulkan}")
