# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("FmIcecast")

# Find libraries
find_package(PkgConfig REQUIRED)
find_package(Volk REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(libairspyhf REQUIRED IMPORTED_TARGET libairspyhf)
pkg_check_modules(shout REQUIRED IMPORTED_TARGET shout)
pkg_check_modules(flac REQUIRED IMPORTED_TARGET flac)
pkg_check_modules(lame REQUIRED IMPORTED_TARGET lame)

# Add SDR++ DSP
add_subdirectory(dsp)

# Add main
add_library (fmice-core STATIC "radio.cpp" "stereo_demod.cpp" "cast.cpp" "circular_buffer.cpp" "codec.cpp" "codecs/codec_flac.cpp" "codecs/codec_mp3.cpp" "rds/rds.cpp" "rds/rds_dec.cpp" "rds/rds_enc.cpp" "stereo_encode.cpp" "stereo_encode.h" "device.h" "devices/device_airspyhf.cpp")
target_link_libraries(fmice-core Volk::volk airspyhf shout FLAC Threads::Threads sdrpp_dsp mp3lame)

# Add executables
add_executable (fmice "main.cpp")
target_link_libraries (fmice fmice-core)

# Add tests
add_subdirectory (tests)