cmake_minimum_required(VERSION 3.16)

project(OpenGothicIOSStartupMsl LANGUAGES CXX)

if(NOT DEFINED SPIRV_CROSS_SOURCE_DIR OR
   NOT EXISTS "${SPIRV_CROSS_SOURCE_DIR}/spirv_msl.cpp")
  message(FATAL_ERROR
    "SPIRV_CROSS_SOURCE_DIR must point at Tempest's vendored SPIRV-Cross")
endif()

add_executable(opengothic-ios-startup-msl
  main.cpp
  "${SPIRV_CROSS_SOURCE_DIR}/spirv_cross.cpp"
  "${SPIRV_CROSS_SOURCE_DIR}/spirv_parser.cpp"
  "${SPIRV_CROSS_SOURCE_DIR}/spirv_cross_parsed_ir.cpp"
  "${SPIRV_CROSS_SOURCE_DIR}/spirv_cfg.cpp"
  "${SPIRV_CROSS_SOURCE_DIR}/spirv_glsl.cpp"
  "${SPIRV_CROSS_SOURCE_DIR}/spirv_msl.cpp")

target_include_directories(opengothic-ios-startup-msl PRIVATE
  "${SPIRV_CROSS_SOURCE_DIR}")
target_compile_features(opengothic-ios-startup-msl PRIVATE cxx_std_17)

set(IOS_STARTUP_TOOL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set_target_properties(opengothic-ios-startup-msl PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${IOS_STARTUP_TOOL_OUTPUT_DIR}")
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${CONFIG}" CONFIG_UPPER)
  set_target_properties(opengothic-ios-startup-msl PROPERTIES
    "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${IOS_STARTUP_TOOL_OUTPUT_DIR}")
endforeach()
