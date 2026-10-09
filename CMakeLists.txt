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
# ggml only recognizes a bare architecture name. Given the preset's "x64,version=...", it silently falls back to
# generic code without AVX2, which makes inference several times slower.
set(_generator_platform "${CMAKE_GENERATOR_PLATFORM}")
string(REGEX REPLACE ",.*$" "" CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
FetchContent_MakeAvailable(whisper)
set(CMAKE_GENERATOR_PLATFORM "${_generator_platform}")

get_target_property(_ggml_cpu_options ggml-cpu COMPILE_OPTIONS)
if("${_ggml_cpu_options}" MATCHES "GGML_CPU_GENERIC")
  message(FATAL_ERROR "ggml was configured without CPU vector instructions; speech recognition would be too slow.")
endif()

# CTranslate2 and SentencePiece translate the recognized text. Both are linked statically, like whisper.cpp.
# CTranslate2 comes from git because its release archives lack the submodules it builds (Ruy, cpu_features, spdlog).
# The Ruy backend needs neither Intel MKL nor an OpenMP runtime; the patch makes its thread pools safe to tear down
# on Windows and lets the C runtime below replace the static one CTranslate2 imposes: see docs/development.md.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(WITH_MKL OFF)
set(WITH_RUY ON)
set(OPENMP_RUNTIME "NONE" CACHE STRING "CTranslate2 without OpenMP" FORCE)
set(BUILD_CLI OFF)
FetchContent_Declare(
  ctranslate2
  GIT_REPOSITORY https://github.com/OpenNMT/CTranslate2.git
  GIT_TAG d44d2d069eb88c7b7804da864c10c201501cb4a9 # v4.8.2
  GIT_SUBMODULES third_party/cpu_features third_party/spdlog third_party/ruy
  GIT_SUBMODULES_RECURSE ON
  PATCH_COMMAND git checkout -- .
  COMMAND git apply --ignore-whitespace "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/ctranslate2-windows.patch"
  EXCLUDE_FROM_ALL
  SYSTEM
)
set(SPM_ENABLE_SHARED OFF)
set(SPM_ENABLE_TCMALLOC OFF)
# The last release that bundles its own subset of Abseil; later ones download the whole library.
FetchContent_Declare(
  sentencepiece
  URL https://github.com/google/sentencepiece/archive/refs/tags/v0.2.1.tar.gz
  URL_HASH SHA256=c1a59e9259c9653ad0ade653dadff074cd31f0a6ff2a11316f67bee4189a8f1b
  EXCLUDE_FROM_ALL
  SYSTEM
)
# Ruy has headers named time.h and cpuinfo.h: with its own directory in the include path, as the OBS template sets
# up for every target, they would hide the system headers of the same name.
set(CMAKE_INCLUDE_CURRENT_DIR FALSE)
FetchContent_MakeAvailable(ctranslate2 sentencepiece)
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
unset(CMAKE_MSVC_RUNTIME_LIBRARY)

# The models are installed with the plugin data but not stored in git.
set(MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/models")
set(VAD_MODEL_FILE "${MODEL_DIR}/ggml-silero-v6.2.0.bin")
set(STT_MODEL_FILE "${MODEL_DIR}/ggml-small-q5_1.bin")
set(MT_MODEL_DIR "${MODEL_DIR}/opus-mt-fr-en")
if(NOT EXISTS "${VAD_MODEL_FILE}")
  file(
    DOWNLOAD
    https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin
    "${VAD_MODEL_FILE}"
    EXPECTED_HASH SHA256=2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987
  )
endif()
if(NOT EXISTS "${STT_MODEL_FILE}")
  message(STATUS "Downloading the speech recognition model (181 MB)")
  file(
    DOWNLOAD
    https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin
    "${STT_MODEL_FILE}"
    EXPECTED_HASH SHA256=ae85e4a935d7a567bd102fe55afc16bb595bdb618e11b2fc7591bc08120411bb
  )
endif()
# Our own conversions of the OPUS-MT models, published as a release of this repository: see docs/development.md.
set(_mt_model_hash_fr-en 281c06fa89a1f0a3bff47ffc3445fc5087c54bfbb50ed37816240a0b9c902b1f)
set(_mt_model_hash_en-fr b852892f3f2cf862f42a16894cecedc8c368e29f153a123d33538a4ce7d7b4fe)
foreach(_mt_pair fr-en en-fr)
  if(NOT EXISTS "${MODEL_DIR}/opus-mt-${_mt_pair}/model.bin")
    message(STATUS "Downloading the ${_mt_pair} translation model (61 MB)")
    set(_mt_model_archive "${CMAKE_CURRENT_BINARY_DIR}/opus-mt-${_mt_pair}-ct2-int8.zip")
    file(
      DOWNLOAD
      https://github.com/ValotKzm/livesubtitles/releases/download/models-v1/opus-mt-${_mt_pair}-ct2-int8.zip
      "${_mt_model_archive}"
      EXPECTED_HASH SHA256=${_mt_model_hash_${_mt_pair}}
    )
    file(ARCHIVE_EXTRACT INPUT "${_mt_model_archive}" DESTINATION "${MODEL_DIR}/opus-mt-${_mt_pair}")
    file(REMOVE "${_mt_model_archive}")
  endif()
endforeach()

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
  PRIVATE
    src/plugin-main.c
    src/subtitle-source.c
    src/audio-ring.c
    src/speech-detector.c
    src/subtitle-engine.c
    src/voice-activity.c
    src/transcriber.c
    src/translator.cpp
)
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE "${sentencepiece_SOURCE_DIR}/src")
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE whisper ctranslate2 sentencepiece-static)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(ENABLE_TESTS)
  enable_testing()

  add_executable(audio-ring-test tests/audio-ring-test.c src/audio-ring.c)
  target_include_directories(audio-ring-test PRIVATE src)
  add_test(NAME audio-ring COMMAND audio-ring-test)

  add_executable(speech-detector-test tests/speech-detector-test.c src/speech-detector.c)
  target_include_directories(speech-detector-test PRIVATE src)
  add_test(NAME speech-detector COMMAND speech-detector-test)

  add_executable(subtitle-engine-test tests/subtitle-engine-test.c src/subtitle-engine.c)
  target_include_directories(subtitle-engine-test PRIVATE src)
  add_test(NAME subtitle-engine COMMAND subtitle-engine-test)

  # Runs the real model on the reference recording shipped with whisper.cpp.
  add_executable(voice-activity-test tests/voice-activity-test.c src/voice-activity.c src/speech-detector.c)
  target_include_directories(voice-activity-test PRIVATE src)
  target_link_libraries(voice-activity-test PRIVATE whisper)
  add_test(NAME voice-activity COMMAND voice-activity-test "${VAD_MODEL_FILE}" "${whisper_SOURCE_DIR}/samples/jfk.wav")

  add_executable(transcriber-test tests/transcriber-test.c src/transcriber.c)
  target_include_directories(transcriber-test PRIVATE src)
  target_link_libraries(transcriber-test PRIVATE whisper)
  add_test(NAME transcriber COMMAND transcriber-test "${STT_MODEL_FILE}" "${whisper_SOURCE_DIR}/samples/jfk.wav")

  # Measurement tool, run by hand with a model and a recording: see docs/development.md.
  add_executable(transcriber-bench tests/transcriber-bench.c src/transcriber.c)
  target_include_directories(transcriber-bench PRIVATE src)
  target_link_libraries(transcriber-bench PRIVATE whisper)

  add_library(translator-lib STATIC src/translator.cpp)
  target_include_directories(translator-lib PUBLIC src)
  target_include_directories(translator-lib SYSTEM PRIVATE "${sentencepiece_SOURCE_DIR}/src")
  target_link_libraries(translator-lib PRIVATE ctranslate2 sentencepiece-static)

  add_executable(translator-test tests/translator-test.cpp)
  target_link_libraries(translator-test PRIVATE translator-lib)
  add_test(NAME translator COMMAND translator-test "${MT_MODEL_DIR}")
  set_tests_properties(translator PROPERTIES TIMEOUT 120)

  # Measurement tool, run by hand with a model directory and a text file: see docs/development.md.
  add_executable(translator-bench tests/translator-bench.c)
  target_link_libraries(translator-bench PRIVATE translator-lib)
endif()
