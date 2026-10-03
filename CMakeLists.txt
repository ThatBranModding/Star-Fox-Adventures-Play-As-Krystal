cmake_minimum_required(VERSION 3.25)
project(play_as_krystal VERSION 0.9.7 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Krystal model and texture resources are bundled with this source package.
# Keeping these resource blobs local makes the mod self-contained and reproducible.
set(KRYSTAL_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(NOT EXISTS "${KRYSTAL_ASSET_DIR}/km0" OR NOT EXISTS "${KRYSTAL_ASSET_DIR}/kt0")
  message(FATAL_ERROR "Missing bundled Krystal assets: assets/km0 and assets/kt0 are required.")
endif()

add_library(play_as_krystal SHARED mod.cpp)
target_link_libraries(play_as_krystal PRIVATE ${CMAKE_DL_LIBS})
target_include_directories(play_as_krystal PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
option(FH_BUILD_TESTS "Build gameplay and compatibility regression checks" OFF)
if(FH_BUILD_TESTS)
  enable_testing()
  add_executable(krystal_cutscene_test tests/cutscene.cpp)
  target_include_directories(krystal_cutscene_test PRIVATE include)
  target_link_libraries(krystal_cutscene_test PRIVATE ${CMAKE_DL_LIBS})
  add_test(NAME krystal_cutscene COMMAND krystal_cutscene_test)
  add_executable(krystal_hitbox_test tests/hitboxes.cpp)
  target_include_directories(krystal_hitbox_test PRIVATE include)
  target_link_libraries(krystal_hitbox_test PRIVATE ${CMAKE_DL_LIBS})
  add_test(NAME krystal_hitbox COMMAND krystal_hitbox_test)
  add_executable(krystal_voice_compat_test tests/voice_compat.cpp)
  target_include_directories(krystal_voice_compat_test PRIVATE include)
  target_link_libraries(krystal_voice_compat_test PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(krystal_voice_compat_test play_as_krystal)
  add_test(NAME krystal_voice_compat COMMAND krystal_voice_compat_test "${CMAKE_BINARY_DIR}/play-as-krystal")
endif()
set_target_properties(play_as_krystal PROPERTIES PREFIX "" OUTPUT_NAME "mod")
if (WIN32)
  set(FH_PLATFORM_DIR "windows-amd64")
elseif (APPLE)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
    set(FH_PLATFORM_DIR "macos-arm64")
  else()
    set(FH_PLATFORM_DIR "macos-x86_64")
  endif()
else()
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
    set(FH_PLATFORM_DIR "linux-arm64")
  else()
    set(FH_PLATFORM_DIR "linux-amd64")
  endif()
endif()
if(APPLE)
  set_target_properties(play_as_krystal PROPERTIES SUFFIX ".so")
endif()
set(FH_PACKAGE_DIR "${CMAKE_BINARY_DIR}/play-as-krystal")
set(FH_LIBRARY_DIR "${FH_PACKAGE_DIR}/lib/${FH_PLATFORM_DIR}")
set_target_properties(play_as_krystal PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${FH_LIBRARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${FH_LIBRARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${FH_LIBRARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${FH_LIBRARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${FH_LIBRARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${FH_LIBRARY_DIR}")
add_custom_command(TARGET play_as_krystal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${FH_PACKAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${FH_PACKAGE_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/mod.json" "${FH_PACKAGE_DIR}/mod.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KRYSTAL_ASSET_DIR}/km0" "${FH_PACKAGE_DIR}/assets/km0"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KRYSTAL_ASSET_DIR}/kt0" "${FH_PACKAGE_DIR}/assets/kt0"
  COMMENT "Packaging Play As Krystal 0.9.7")

add_custom_command(TARGET play_as_krystal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/overlay" "${FH_PACKAGE_DIR}/overlay"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${FH_PACKAGE_DIR}/README.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${FH_PACKAGE_DIR}/LICENSE")
