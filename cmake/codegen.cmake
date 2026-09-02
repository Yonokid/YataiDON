find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SKIN_CONFIG_JSON  "${CMAKE_SOURCE_DIR}/Skins/PyTaikoGreen/Graphics/skin_config.json")
set(SKIN_CONFIG_GEN_H "${CMAKE_BINARY_DIR}/generated/skin_config_generated.h")

add_custom_command(
    OUTPUT  ${SKIN_CONFIG_GEN_H}
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/gen_skin_config.py"
            "${SKIN_CONFIG_JSON}"
            "${SKIN_CONFIG_GEN_H}"
    DEPENDS "${SKIN_CONFIG_JSON}" "${CMAKE_SOURCE_DIR}/tools/gen_skin_config.py"
    COMMENT "Generating skin_config_generated.h from skin_config.json"
)
add_custom_target(skin_config_gen DEPENDS ${SKIN_CONFIG_GEN_H})

set(TEXTURE_IDS_GEN_H "${CMAKE_BINARY_DIR}/generated/texture_ids_generated.h")

# The TexID enum is the union over EVERY skin's texture.json, so a child skin can
# introduce texture names (and whole subsets) of its own without touching the base
# skin. YATAIDON_EXTRA_SKIN_DIRS is a ';'-list of extra <skin>/Graphics dirs for
# skins that live outside Skins/ (e.g. installed next to the executable).
set(YATAIDON_EXTRA_SKIN_DIRS "" CACHE STRING
    "Extra skin Graphics directories to include in the generated TexID enum")

file(GLOB SKIN_GRAPHICS_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/Skins/*/Graphics")
set(ALL_SKIN_GRAPHICS_DIRS "")
foreach(dir IN LISTS SKIN_GRAPHICS_DIRS YATAIDON_EXTRA_SKIN_DIRS)
    if(IS_DIRECTORY "${dir}")
        list(APPEND ALL_SKIN_GRAPHICS_DIRS "${dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES ALL_SKIN_GRAPHICS_DIRS)

set(TEXTURE_JSON_FILES "")
foreach(dir IN LISTS ALL_SKIN_GRAPHICS_DIRS)
    file(GLOB_RECURSE _tex_jsons "${dir}/*/*/texture.json")
    list(APPEND TEXTURE_JSON_FILES ${_tex_jsons})
endforeach()

add_custom_command(
    OUTPUT  ${TEXTURE_IDS_GEN_H}
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/gen_textures.py"
            ${ALL_SKIN_GRAPHICS_DIRS}
            "${TEXTURE_IDS_GEN_H}"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/gen_textures.py" ${TEXTURE_JSON_FILES}
    COMMENT "Generating texture_ids_generated.h from skin texture.json files"
)
add_custom_target(texture_ids_gen DEPENDS ${TEXTURE_IDS_GEN_H})
