# Mesa permits NVK without libelf when VK_NVX_binary_import is disabled.
# Its release SDK config currently requires libelf unconditionally, so also
# accept a Vulkan-only SDK built with the public Switch toolchain.
set(YATAIDON_MESA_LIBRARY "" CACHE FILEPATH "Optional mesa-switch libvulkan.a override")
if(YATAIDON_MESA_LIBRARY)
  if(NOT EXISTS "${YATAIDON_MESA_LIBRARY}")
    message(FATAL_ERROR "YATAIDON_MESA_LIBRARY does not exist: ${YATAIDON_MESA_LIBRARY}")
  endif()
  set(_switch_vulkan "${YATAIDON_MESA_LIBRARY}")
else()
  find_library(_switch_vulkan NAMES vulkan PATHS "${SWITCH_MESA_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
endif()
find_path(_switch_vulkan_headers NAMES vulkan/vulkan.h
  PATHS "${SWITCH_MESA_PREFIX}/include" NO_DEFAULT_PATH REQUIRED)
set(CMAKE_LINK_GROUP_USING_RESCAN "LINKER:--start-group" "LINKER:--end-group")
set(CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED TRUE)
add_library(Vulkan::Vulkan INTERFACE IMPORTED)
set(_switch_vulkan_libraries "${_switch_vulkan}")
foreach(lib expat zstd z nx)
  find_library(_switch_${lib} NAMES ${lib} REQUIRED)
  list(APPEND _switch_vulkan_libraries "${_switch_${lib}}")
endforeach()
find_library(_switch_elf NAMES elf)
if(_switch_elf)
  list(APPEND _switch_vulkan_libraries "${_switch_elf}")
endif()
list(APPEND _switch_vulkan_libraries stdc++ m)
list(JOIN _switch_vulkan_libraries "," _switch_vulkan_group)
set_target_properties(Vulkan::Vulkan PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${_switch_vulkan_headers}"
  INTERFACE_LINK_LIBRARIES "$<LINK_GROUP:RESCAN,${_switch_vulkan_group}>"
  INTERFACE_LINK_OPTIONS "-Wl,-u,vk_icdGetInstanceProcAddr;-Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion;-pthread")
