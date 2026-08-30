cmake_minimum_required(VERSION 3.16)

foreach(REQUIRED_VAR
    STARTUP_DIR TRIANGLE_SPV DOWNSCALE_SPV STARTUP_MSL_TOOL
    XCRUN METAL_SDK METAL_TARGET)
  if(NOT DEFINED ${REQUIRED_VAR} OR "${${REQUIRED_VAR}}" STREQUAL "")
    message(FATAL_ERROR "Missing ${REQUIRED_VAR} for the iOS startup metallibs")
  endif()
endforeach()

if(NOT EXISTS "${STARTUP_MSL_TOOL}")
  message(FATAL_ERROR "Native startup MSL generator is missing: ${STARTUP_MSL_TOOL}")
endif()

file(MAKE_DIRECTORY "${STARTUP_DIR}")

function(run_checked DESCRIPTION)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE COMMAND_RESULT
    ERROR_VARIABLE COMMAND_ERROR)
  if(NOT COMMAND_RESULT EQUAL 0)
    string(STRIP "${COMMAND_ERROR}" COMMAND_ERROR)
    message(FATAL_ERROR "${DESCRIPTION} failed: ${COMMAND_ERROR}")
  endif()
endfunction()

function(compile_startup_shader NAME STAGE SPV VERSION METAL_STANDARD)
  set(BASE_NAME "OpenGothic${NAME}_${VERSION}")
  set(METAL_SOURCE "${STARTUP_DIR}/${BASE_NAME}.mslsrc")
  set(MAX_PROFILE_SOURCE "${STARTUP_DIR}/${BASE_NAME}.max-profile.mslsrc")
  set(AIR_OBJECT "${STARTUP_DIR}/${BASE_NAME}.air")
  set(METAL_LIBRARY "${STARTUP_DIR}/${BASE_NAME}.metallib")

  # Tempest's two real iOS device profiles are represented by the endpoints:
  # Tier1/no-rich-descriptor/native image atomics and
  # Tier2/rich-descriptor/emulated image atomics with a large alignment.
  # These two startup shaders must not depend on any of those capabilities.
  run_checked("Canonical SPIRV-Cross generation for ${NAME} MSL ${VERSION}"
    "${STARTUP_MSL_TOOL}"
      --input "${SPV}"
      --output "${METAL_SOURCE}"
      --stage "${STAGE}"
      --msl-version "${VERSION}"
      --argument-buffers-tier 0
      --runtime-array-rich-descriptor 0
      --r32ui-linear-texture-alignment 4
      --r32ui-alignment-constant-id 65535)
  run_checked("Max-profile SPIRV-Cross generation for ${NAME} MSL ${VERSION}"
    "${STARTUP_MSL_TOOL}"
      --input "${SPV}"
      --output "${MAX_PROFILE_SOURCE}"
      --stage "${STAGE}"
      --msl-version "${VERSION}"
      --argument-buffers-tier 1
      --runtime-array-rich-descriptor 1
      --r32ui-linear-texture-alignment 4096
      --r32ui-alignment-constant-id 0)

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files
      "${METAL_SOURCE}" "${MAX_PROFILE_SOURCE}"
    RESULT_VARIABLE PROFILE_COMPARE_RESULT)
  if(NOT PROFILE_COMPARE_RESULT EQUAL 0)
    message(FATAL_ERROR
      "${NAME} MSL ${VERSION} depends on the runtime Metal capability profile; keep it on Tempest's runtime compilation path")
  endif()

  run_checked("Metal compilation for ${NAME} MSL ${VERSION}"
    "${XCRUN}" -sdk "${METAL_SDK}" metal
      -x metal "-std=${METAL_STANDARD}" -target "${METAL_TARGET}"
      -c "${METAL_SOURCE}" -o "${AIR_OBJECT}")
  run_checked("Metallib link for ${NAME} MSL ${VERSION}"
    "${XCRUN}" -sdk "${METAL_SDK}" metallib
      "${AIR_OBJECT}" -o "${METAL_LIBRARY}")
endfunction()

foreach(VERSION_AND_STANDARD
    "20400;ios-metal2.4"
    "30000;metal3.0"
    "30100;metal3.1")
  list(GET VERSION_AND_STANDARD 0 MSL_VERSION)
  list(GET VERSION_AND_STANDARD 1 METAL_STANDARD)
  compile_startup_shader(Triangle vertex   "${TRIANGLE_SPV}"
    "${MSL_VERSION}" "${METAL_STANDARD}")
  compile_startup_shader(Downscale fragment "${DOWNSCALE_SPV}"
    "${MSL_VERSION}" "${METAL_STANDARD}")
endforeach()

set(HASH_HEADER
  "${STARTUP_DIR}/opengothic_ios_startup_hashes.h")
set(HASH_HEADER_TEMP "${HASH_HEADER}.tmp")
set(HASH_RECORDS "")
set(HASH_RECORD_COUNT 0)

foreach(MSL_VERSION 20400 30000 30100)
  foreach(SHADER_NAME Triangle Downscale)
    set(RESOURCE_NAME "OpenGothic${SHADER_NAME}_${MSL_VERSION}")
    set(METAL_LIBRARY "${STARTUP_DIR}/${RESOURCE_NAME}.metallib")
    file(SHA256 "${METAL_LIBRARY}" METAL_LIBRARY_SHA256)
    set(HASH_BYTES "")
    foreach(BYTE_OFFSET RANGE 0 62 2)
      string(SUBSTRING "${METAL_LIBRARY_SHA256}" ${BYTE_OFFSET} 2 HASH_BYTE)
      string(APPEND HASH_BYTES "0x${HASH_BYTE},")
    endforeach()
    string(APPEND HASH_RECORDS
      "    {\"${RESOURCE_NAME}\",${MSL_VERSION}u,{{${HASH_BYTES}}}},\n")
    math(EXPR HASH_RECORD_COUNT "${HASH_RECORD_COUNT}+1")
  endforeach()
endforeach()

file(WRITE "${HASH_HEADER_TEMP}"
  "#pragma once\n\n"
  "#include <array>\n"
  "#include <cstddef>\n"
  "#include <cstdint>\n\n"
  "namespace OpenGothic::IOSStartupShaders {\n\n"
  "struct ExpectedMetalLibraryHash final {\n"
  "  const char* resourceName;\n"
  "  uint32_t mslVersion;\n"
  "  std::array<uint8_t,32> sha256;\n"
  "  };\n\n"
  "inline constexpr std::array<ExpectedMetalLibraryHash,${HASH_RECORD_COUNT}> "
  "ExpectedMetalLibraryHashes = {{\n"
  "${HASH_RECORDS}"
  "  }};\n\n"
  "}\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${HASH_HEADER_TEMP}" "${HASH_HEADER}"
  RESULT_VARIABLE HASH_HEADER_COPY_RESULT
  ERROR_VARIABLE HASH_HEADER_COPY_ERROR)
file(REMOVE "${HASH_HEADER_TEMP}")
if(NOT HASH_HEADER_COPY_RESULT EQUAL 0)
  string(STRIP "${HASH_HEADER_COPY_ERROR}" HASH_HEADER_COPY_ERROR)
  message(FATAL_ERROR
    "Writing the startup metallib SHA-256 header failed: ${HASH_HEADER_COPY_ERROR}")
endif()
