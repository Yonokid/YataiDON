file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/shaders/spirv")
foreach(shader vs_2d vs_sprite fs_2d fs_sprite fs_colortransform vs_mesh vs_outline vs_mesh_skin vs_outline_skin
    fs_cutout fs_mesh fs_null fs_outline fs_outline_fxaa)
  if(shader MATCHES "^vs_")
    set(type vertex)
  else()
    set(type fragment)
  endif()
  set(output "${CMAKE_BINARY_DIR}/generated/shaders/spirv/${shader}.sc.bin.h")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${YATAIDON_SHADERC}" -f "${game_root}/shader/bgfx/${shader}.sc" -o "${output}"
      --type ${type} --platform linux -p spirv --bin2c ${shader}_spv
      --varyingdef "${game_root}/shader/bgfx/varying.def.sc" -i "${vendor}/bgfx.cmake/bgfx/src"
    DEPENDS "${game_root}/shader/bgfx/${shader}.sc" "${game_root}/shader/bgfx/varying.def.sc"
      "${game_root}/shader/bgfx/skin.sh" "${game_root}/shader/bgfx/color_transform.sh" VERBATIM)
  target_sources(YataiDON PRIVATE "${output}")
endforeach()
