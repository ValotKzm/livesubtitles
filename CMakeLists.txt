cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)
option(ENABLE_TESTS "Build unit tests" ON)

# whisper.cpp provides the Silero voice activity detection (and later speech recognition). It is fetched before the
# OBS compiler configuration so that it builds with its own flags, and linked statically into the plugin. The CPU
# baseline (AVX2) and the absence of OpenMP are deliberate: see docs/development.md.
include(FetchContent)
# whisper.cpp declares an older CMake version, under which option() would discard the plain variables set below.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Link whisper.cpp statically" FORCE)
set(WHISPER_BUILD_TESTS OFF)
set(WHISPER_BUILD_EXAMPLES OFF)
set(WHISPER_BUILD_SERVER OFF)
set(GGML_NATIVE OFF)
set(GGML_OPENMP OFF)
set(GGML_CCACHE OFF)
FetchContent_Declare(
  whisper
  URL https://github.com/ggml-org/whisper.cpp/archive/refs/tags/v1.9.5.tar.gz
  URL_HASH SHA256=ff1a9053feb509ff9d7729703355541ae9690073a6b1c40eb692c962e0dc1720
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(whisper)

# The voice activity model is installed with the plugin data but not stored in git.
set(VAD_MODEL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/data/models/ggml-silero-v6.2.0.bin")
if(NOT EXISTS "${VAD_MODEL_FILE}")
  file(
    DOWNLOAD
    https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin
    "${VAD_MODEL_FILE}"
    EXPECTED_HASH SHA256=2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987
  )
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.c src/subtitle-source.c src/audio-ring.c src/speech-detector.c src/voice-activity.c
)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE whisper)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(ENABLE_TESTS)
  enable_testing()

  add_executable(audio-ring-test tests/audio-ring-test.c src/audio-ring.c)
  target_include_directories(audio-ring-test PRIVATE src)
  add_test(NAME audio-ring COMMAND audio-ring-test)

  add_executable(speech-detector-test tests/speech-detector-test.c src/speech-detector.c)
  target_include_directories(speech-detector-test PRIVATE src)
  add_test(NAME speech-detector COMMAND speech-detector-test)

  # Runs the real model on the reference recording shipped with whisper.cpp.
  add_executable(voice-activity-test tests/voice-activity-test.c src/voice-activity.c src/speech-detector.c)
  target_include_directories(voice-activity-test PRIVATE src)
  target_link_libraries(voice-activity-test PRIVATE whisper)
  add_test(NAME voice-activity COMMAND voice-activity-test "${VAD_MODEL_FILE}" "${whisper_SOURCE_DIR}/samples/jfk.wav")
endif()
