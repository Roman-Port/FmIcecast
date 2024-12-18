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

# Add SDR++ DSP
add_subdirectory(dsp)

# Add main
add_executable (fmice "main.cpp" "radio.cpp" "stereo_demod.cpp" "cast.cpp" "circular_buffer.cpp" "codec.cpp" "codecs/codec_flac.cpp" "rds/rds.cpp" "rds/rds_dec.cpp" "rds/rds_enc.cpp")
#add_executable (fmice "rds/rds_test.cpp" "rds/rds.cpp" "rds/rds_dec.cpp" "rds/rds_enc.cpp" "rds/rds_enc.h")

# Link
target_link_libraries(fmice Volk::volk airspyhf shout FLAC Threads::Threads sdrpp_dsp)
#target_link_libraries(fmice Volk::volk Threads::Threads sdrpp_dsp)