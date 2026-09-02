cmake_minimum_required(VERSION 3.16)

project(OpenGothic LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/opengothic)
set(CMAKE_DEBUG_POSTFIX "")
set(CMAKE_RELWITHDEBINFO_POSTFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_RPATH ON)

if(ANDROID)
  # First-light deliberately proves the platform layer before compiling the
  # whole game. Keep the core target available for the next migration stage,
  # but outside Android's default target graph until Gothic2Notr links it.
  add_library(${PROJECT_NAME} STATIC EXCLUDE_FROM_ALL)
  # GameActivity loads this target with System.loadLibrary("Gothic2Notr").
  add_library(Gothic2Notr SHARED)
  set(OPENGOTHIC_ANDROID_BENCHMARK_SAVE "0" CACHE STRING
      "Save slot autoloaded by Android benchmark builds (0 disables autoload)")
  target_compile_definitions(Gothic2Notr PRIVATE
    OPENGOTHIC_ANDROID_BENCHMARK_SAVE=${OPENGOTHIC_ANDROID_BENCHMARK_SAVE})
else()
  add_library(${PROJECT_NAME} STATIC)
  add_executable(Gothic2Notr)
  add_executable(Spacer)
endif()

option(OPENGOTHIC_NPC_ANIMATION_CULLING
       "Skip full skeletal pose updates for distant offscreen NPCs" OFF)
if(OPENGOTHIC_NPC_ANIMATION_CULLING)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    OPENGOTHIC_NPC_ANIMATION_CULLING=1)
endif()

option(OPENGOTHIC_RENDER_DIAGNOSTICS
       "Enable test-only renderer isolation profiles" OFF)
if(OPENGOTHIC_RENDER_DIAGNOSTICS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    OPENGOTHIC_RENDER_DIAGNOSTICS=1)
endif()

if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4800")
endif()

if(NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wconversion -Wno-strict-aliasing -Werror)
  if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "8.0")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-class-memaccess)
  endif()
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "7.1" AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-format-truncation)
  endif()
  if(UNIX AND NOT ANDROID)
    # -rdynamic is needed to allow obtaining meaningful backtraces
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU") # gcc
    # required by pthread_setname_np()
    add_definitions(-D_GNU_SOURCE)
  endif()
endif()

if(WIN32)
  add_definitions(-DNOMINMAX)
  if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup" )
  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lpthread")
  else()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -lstdc++fs")
  endif()
elseif(IOS)
  enable_language(OBJCXX)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
  set_directory_properties("${CMAKE_SOURCE_DIR}" PROPERTIES
    PROPERTY XCODE_STARTUP_PROJECT "${PROJECT_NAME}")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "opengothic.gothic2"
    MACOSX_BUNDLE_BUNDLE_VERSION 1
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
elseif(ANDROID)
  find_package(game-activity REQUIRED CONFIG)
  # main_android.cpp initializes Tempest's Android backend. The migration API
  # is internal to Tempest and intentionally is not part of its public include
  # directory yet.
  target_include_directories(Gothic2Notr PRIVATE lib/Tempest/Engine)
elseif(APPLE)
  enable_language(OBJCXX)
endif()

if(IOS)
  file(GLOB_RECURSE ObjCSOURCES "common/*.mm")
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework AppKit")
  file(GLOB_RECURSE ObjCSOURCES "common/*.mm")
endif()

## OpenGothic

# sources
file(GLOB_RECURSE OPENGOTHIC_SOURCES "common/*.h" "common/*.cpp")
file(GLOB_RECURSE SPACER_SOURCES "spacer/*.h" "spacer/*.cpp")

target_sources(${PROJECT_NAME} PRIVATE ${OPENGOTHIC_SOURCES} ${ObjCSOURCES})

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} GothicShaders)

# editor icons and such
if(NOT ANDROID)
  add_subdirectory(spacer/assets)
endif()

include_directories("common")

## submodule dependencies

# edd-dbg
if(WIN32)
  add_subdirectory(lib/edd-dbg)
  include_directories(lib/edd-dbg/include)
  target_link_libraries(${PROJECT_NAME} edd_dbg)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} shlwapi DbgHelp)
elseif(ANDROID)
  # GameActivity's JNI entry points are referenced from Java, not native code.
  # Keep the complete static glue archive in release builds using
  # --gc-sections, otherwise initializeNativeCode is stripped at link time.
  target_link_libraries(Gothic2Notr PRIVATE
    -Wl,--whole-archive
    game-activity::game-activity_static
    -Wl,--no-whole-archive
    android
    log)
elseif(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread -ldl)
endif()

# ZenKit
set(BUILD_SQUISH_WITH_SSE2     OFF CACHE INTERNAL "")
set(ZK_ENABLE_ASAN             OFF CACHE INTERNAL "")
set(ZK_ENABLE_DEPRECATION      OFF CACHE INTERNAL "")

add_subdirectory(lib/ZenKit)
target_link_libraries(${PROJECT_NAME} zenkit)

# Keep Gothic's BC-compressed textures compressed on mobile GPUs that expose
# ASTC but not BC. The transcoder remains capability-gated at runtime, so BC
# capable devices and every non-Android platform keep their existing path.
if(ANDROID)
  set(ASTCENC_CLI          OFF CACHE INTERNAL "")
  set(ASTCENC_SHAREDLIB    OFF CACHE INTERNAL "")
  set(ASTCENC_UNITTEST     OFF CACHE INTERNAL "")
  set(ASTCENC_DECOMPRESSOR OFF CACHE INTERNAL "")
  set(ASTCENC_DIAGNOSTICS  OFF CACHE INTERNAL "")
  set(ASTCENC_WERROR       OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_NATIVE   OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_NONE     OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_AVX2     OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_SSE41    OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_SSE2     OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_SVE_256  OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_SVE_128  OFF CACHE INTERNAL "")
  set(ASTCENC_ISA_NEON     OFF CACHE INTERNAL "")

  if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(ASTCENC_ISA_NEON ON CACHE INTERNAL "")
    set(ASTCENC_LIB astcenc-neon-static)
  elseif(ANDROID_ABI STREQUAL "x86_64")
    set(ASTCENC_ISA_SSE41 ON CACHE INTERNAL "")
    set(ASTCENC_LIB astcenc-sse4.1-static)
  else()
    message(FATAL_ERROR "astcenc: unsupported ANDROID_ABI '${ANDROID_ABI}'")
  endif()

  add_subdirectory(lib/astcenc)
  target_include_directories(${PROJECT_NAME} PRIVATE lib/astcenc/Source)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_ASTCENC=1)
  target_link_libraries(${PROJECT_NAME} ${ASTCENC_LIB})
endif()

# dmusic
set(DM_BUILD_STATIC            ON  CACHE INTERNAL "")
set(DM_ENABLE_ASAN             OFF CACHE INTERNAL "")

add_subdirectory(lib/dmusic)
target_link_libraries(${PROJECT_NAME} dmusic)

# TinySoundFont
add_definitions(-DTSF_NO_STDIO)
include_directories(lib/TinySoundFont)

# Rapidjson
include_directories(lib/rapidjson/include)

# Tempest
if(WIN32)
  set(TEMPEST_BUILD_SHARED ON  CACHE INTERNAL "")
else()
  set(TEMPEST_BUILD_SHARED OFF CACHE INTERNAL "")
endif()
add_subdirectory(lib/Tempest/Engine)
include_directories(lib/Tempest/Engine/include)
target_link_libraries(${PROJECT_NAME} Tempest)

# zip
include_directories(lib/miniz)
add_definitions(-DMINIZ_NO_STDIO)
add_library(miniz STATIC EXCLUDE_FROM_ALL "lib/miniz/miniz.h" "lib/miniz/miniz.c")
target_link_libraries(${PROJECT_NAME} miniz)

# bullet physics
set(BULLET2_MULTITHREADING       ON  CACHE INTERNAL "")
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON  CACHE INTERNAL "")
set(USE_GRAPHICAL_BENCHMARK      OFF CACHE INTERNAL "")
set(BUILD_BULLET2_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_OPENGL3_DEMOS          OFF CACHE INTERNAL "") # No samples
set(BUILD_UNIT_TESTS             OFF CACHE INTERNAL "") # No tests
set(BUILD_CPU_DEMOS              OFF CACHE INTERNAL "") # No samples
set(BUILD_EXTRAS                 OFF CACHE INTERNAL "") # No bugs
set(GLFW_BUILD_EXAMPLES          OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS             OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS              OFF CACHE INTERNAL "")
set(BUILD_BULLET3                OFF CACHE INTERNAL "") # Can use bullet2, bullet3 wants to build examples...
add_subdirectory(lib/bullet3)
if(NOT MSVC)
  target_compile_options(BulletDynamics         PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-argument-outside-range)
  target_compile_options(Bullet3Common          PRIVATE -Wno-argument-outside-range)
  target_compile_options(BulletCollision        PRIVATE -Wno-argument-outside-range)
  target_compile_options(LinearMath             PRIVATE -Wno-argument-outside-range)
endif()
if(NOT MSVC AND NOT APPLE AND NOT ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  target_compile_options(BulletDynamics         PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletInverseDynamics  PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletSoftBody         PRIVATE -Wno-stringop-overflow)
  target_compile_options(Bullet3Common          PRIVATE -Wno-stringop-overflow)
  target_compile_options(BulletCollision        PRIVATE -Wno-stringop-overflow)
  target_compile_options(LinearMath             PRIVATE -Wno-stringop-overflow)
endif()

include_directories(lib/bullet3/src)
target_link_libraries(${PROJECT_NAME} BulletDynamics BulletCollision LinearMath)

# script for launching in binary directory
if(WIN32)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/scripts/Gothic2Notr.bat
        ${CMAKE_CURRENT_BINARY_DIR}/opengothic/Gothic2Notr.bat)
endif()

# in debug mode, enable sanitizers
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  add_compile_options(-fsanitize=address)
  add_compile_options(-fsanitize=leak)
  add_link_options   (-fsanitize=address)
  add_link_options   (-fsanitize=leak)
endif()

# the game
target_sources(Gothic2Notr PRIVATE game/gamemain.cpp)
if(ANDROID)
  target_sources(Gothic2Notr PRIVATE game/main_android.cpp)
  target_link_libraries(Gothic2Notr PRIVATE OpenGothic)
else()
  target_sources(Gothic2Notr PRIVATE game/main.cpp icon.rc)
  target_link_libraries(Gothic2Notr PRIVATE OpenGothic)
endif()

# editor
if(NOT ANDROID)
  target_sources(Spacer PRIVATE ${SPACER_SOURCES} spacer/main.cpp)
  target_include_directories(Spacer PRIVATE "spacer")
  target_link_libraries(Spacer PRIVATE OpenGothic SpacerAssets)
endif()

# installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
