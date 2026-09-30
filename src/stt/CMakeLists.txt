# xournalai-stt: a small helper process that turns speech into text locally with whisper.cpp. The app talks to it
# with JSON lines over stdin/stdout (see main.cpp), so a crash here never takes the app down.

include(FetchContent)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_SDL2 OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE ON CACHE BOOL "" FORCE)  # a personal build: use this CPU's instruction set
FetchContent_Declare(
        whisper
        GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
        GIT_TAG v1.9.4
        GIT_SHALLOW TRUE)
# Build whisper as static libraries (one self-contained helper binary), without changing how the rest of the project
# is built. CMP0077: let whisper's option(BUILD_SHARED_LIBS) take the value set here
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(_stt_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(whisper)
set(BUILD_SHARED_LIBS ${_stt_shared})

add_executable(xournalai-stt main.cpp)
pkg_check_modules(SndFile REQUIRED IMPORTED_TARGET "sndfile >= 1.0.25")  # recordings (.ogg) for audio_transcribe
target_link_libraries(xournalai-stt PRIVATE whisper PkgConfig::PortAudio PkgConfig::SndFile nlohmann_json::nlohmann_json)
target_compile_features(xournalai-stt PRIVATE cxx_std_20)
install(TARGETS xournalai-stt RUNTIME DESTINATION bin COMPONENT xournalpp)

# The sample the tests transcribe (John F. Kennedy: "... ask not what your country can do for you ...")
set(XOURNALAI_STT_SAMPLE "${whisper_SOURCE_DIR}/samples/jfk.wav" CACHE INTERNAL "")
