cmake_minimum_required(VERSION 3.24)
project(ScreenTab VERSION 0.1.1 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "ScreenTab v0.1 supports Windows 11 x64 only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(SCREENTAB_SYMBOL_RUNTIME_DIR "${CMAKE_BINARY_DIR}/symbol-runtime" CACHE PATH
    "Directory containing the Microsoft x64 DbgHelp symbol runtime")
set(SCREENTAB_SYMBOL_RUNTIME_FILES
    "${SCREENTAB_SYMBOL_RUNTIME_DIR}/dbghelp.dll"
    "${SCREENTAB_SYMBOL_RUNTIME_DIR}/symsrv.dll"
    "${SCREENTAB_SYMBOL_RUNTIME_DIR}/msdia140.dll")
foreach(runtime_file IN LISTS SCREENTAB_SYMBOL_RUNTIME_FILES)
  if(NOT EXISTS "${runtime_file}")
    message(FATAL_ERROR "Missing ${runtime_file}. Run scripts/fetch_symbol_runtime.ps1 first.")
  endif()
endforeach()

include(FetchContent)
FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG v1.3.4
)
FetchContent_MakeAvailable(minhook)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/controller/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/ScreenTabVersion.rc
  @ONLY
)

add_executable(ScreenTab WIN32
  src/controller/main.cpp
  src/controller/symbols.cpp
  src/controller/injector.cpp
  src/controller/app.rc
  ${CMAKE_CURRENT_BINARY_DIR}/ScreenTabVersion.rc
)
target_include_directories(ScreenTab PRIVATE src)
target_compile_definitions(ScreenTab PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(ScreenTab PRIVATE dbghelp shell32 shlwapi version user32 advapi32)
add_custom_command(TARGET ScreenTab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${SCREENTAB_SYMBOL_RUNTIME_FILES} $<TARGET_FILE_DIR:ScreenTab>
  VERBATIM)

add_library(ScreenTabHook SHARED
  src/hook/dllmain.cpp
  src/hook/alt_tab_hook.cpp
)
target_include_directories(ScreenTabHook PRIVATE src)
target_compile_definitions(ScreenTabHook PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(ScreenTabHook PRIVATE minhook user32 version)
set_target_properties(ScreenTabHook PROPERTIES OUTPUT_NAME "ScreenTabHook")

install(TARGETS ScreenTab ScreenTabHook RUNTIME DESTINATION . LIBRARY DESTINATION .)
install(FILES ${SCREENTAB_SYMBOL_RUNTIME_FILES} DESTINATION .)
