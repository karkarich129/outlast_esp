cmake_minimum_required(VERSION 3.20)
project(OutlastInternal CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui")
set(MINHOOK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/minhook")

set(MINHOOK_SOURCES
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_DIR}/src/hde/hde64.c
    ${MINHOOK_DIR}/src/hde/hde32.c
)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

set(SOURCE_FILES
    src/dllmain.cpp
    src/hooks/hooks.cpp
    src/game/esp.cpp
    ${IMGUI_SOURCES}
    ${MINHOOK_SOURCES}
)

add_library(OutlastESP SHARED ${SOURCE_FILES})

target_include_directories(OutlastESP PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${MINHOOK_DIR}/include
)

target_link_libraries(OutlastESP PRIVATE
    d3d9
    d3d11
    dxgi
)

if(MSVC)
    target_compile_options(OutlastESP PRIVATE /W3 /MP /utf-8)
endif()
