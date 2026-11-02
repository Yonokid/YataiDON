enable_language(OBJCXX)
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/platform/platform_ios.mm")
# Apple's Mach-O headers define PLATFORM_IOS even in macOS builds.
target_compile_definitions(${PROJECT_NAME} PRIVATE YATAIDON_PLATFORM_IOS)
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
  OBJCXX_STANDARD 23
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
  # CMake leaves INSTALL_PATH empty, so Archive would produce an empty
  # Generic Xcode Archive instead of an iOS App Archive.
  XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
  XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
  XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
  # App Store Connect expects a dSYM for every archive, Release included.
  XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES"
  XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym"
  # Build into Xcode's standard products folder instead of the shared bin/
  # directory. Archive then uses its own folder: a fixed path let Archive leave
  # a symlink to its (later deleted) install copy where the next build writes
  # the app, and kept the dSYM out of the archive.
  XCODE_ATTRIBUTE_CONFIGURATION_BUILD_DIR "$(SYMROOT)/$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)"
  XCODE_GENERATE_SCHEME TRUE)
# A single 1024x1024 AppIcon; actool generates the per-device sizes.
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
set_source_files_properties("${CMAKE_SOURCE_DIR}/ios/Assets.xcassets" PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources)
if(IOS_DEVELOPMENT_TEAM)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${IOS_DEVELOPMENT_TEAM}")
endif()
# Leave signing enabled in the generated project so selecting a team in Xcode
# works. Unsigned command-line checks disable signing for that build only.

set(IOS_SONGS_DIR "${CMAKE_SOURCE_DIR}/Songs" CACHE PATH "Songs included in the iOS app")
# Use a resource directory, not thousands of individual Xcode file entries.
# Refresh it on every build, including asset-only edits.
add_custom_target(ios_assets
  COMMAND ${CMAKE_COMMAND}
    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DSONGS_DIR=${IOS_SONGS_DIR}"
    "-DDEST_DIR=${CMAKE_BINARY_DIR}/ios-resources/GameData"
    -P "${CMAKE_SOURCE_DIR}/cmake/ios_assets.cmake"
  VERBATIM)
add_dependencies(${PROJECT_NAME} ios_assets)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ios-resources/GameData")
if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
  message(FATAL_ERROR "iOS builds require the Xcode generator (folder-reference resource copying is Xcode-only); got '${CMAKE_GENERATOR}'.")
endif()
target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/ios-resources/GameData")
set_source_files_properties("${CMAKE_BINARY_DIR}/ios-resources/GameData" PROPERTIES
  MACOSX_PACKAGE_LOCATION Resources)
