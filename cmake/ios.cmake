enable_language(OBJCXX)
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/platform/ios.mm")
target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_IOS)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static
  "-framework UIKit" "-framework Foundation" "-framework OpenGLES"
  "-framework AudioToolbox" "-framework AVFoundation" "-framework CoreMedia"
  "-framework CoreVideo" "-framework QuartzCore" "-framework GameController"
  z bz2 iconv)
# Objective-C categories in static SDL must be retained by the linker.
target_link_options(${PROJECT_NAME} PRIVATE -ObjC)

set(IOS_BUNDLE_IDENTIFIER "com.yataidon.app" CACHE STRING "iOS application bundle identifier")
set(IOS_DEVELOPMENT_TEAM "" CACHE STRING "Apple development team for signing")
set_target_properties(${PROJECT_NAME} PROPERTIES
  OBJCXX_STANDARD 20
  OBJCXX_STANDARD_REQUIRED YES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
  MACOSX_BUNDLE_GUI_IDENTIFIER "${IOS_BUNDLE_IDENTIFIER}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${IOS_BUNDLE_IDENTIFIER}"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
  XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
  XCODE_GENERATE_SCHEME TRUE)
if(IOS_DEVELOPMENT_TEAM)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${IOS_DEVELOPMENT_TEAM}")
else()
  set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
endif()

set(IOS_SONGS_DIR "${CMAKE_SOURCE_DIR}/Songs" CACHE PATH "Songs included in the iOS app")
if(NOT EXISTS "${YATAIDON_SKINS_DIR}/PyTaikoGreen/Graphics/skin_config.json")
  message(FATAL_ERROR "PyTaikoGreen assets are missing. Populate the skin submodule or set YATAIDON_SKINS_DIR to a complete Skins directory.")
endif()
# Use a resource directory, not thousands of individual Xcode file entries.
# Refresh it on every build, including asset-only edits.
add_custom_target(ios_assets
  COMMAND ${CMAKE_COMMAND}
    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DSKINS_DIR=${YATAIDON_SKINS_DIR}"
    "-DSONGS_DIR=${IOS_SONGS_DIR}"
    "-DDEST_DIR=${CMAKE_BINARY_DIR}/ios-resources/GameData"
    -P "${CMAKE_SOURCE_DIR}/cmake/ios_assets.cmake"
  VERBATIM)
add_dependencies(${PROJECT_NAME} ios_assets)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ios-resources/GameData")
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/ios-resources/GameData")
set_source_files_properties("${CMAKE_BINARY_DIR}/ios-resources/GameData" PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources)
