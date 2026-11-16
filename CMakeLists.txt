cmake_minimum_required(VERSION 3.20)
project(skeet_menu_standalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

add_executable(skeet_menu_standalone WIN32
    src/main.cpp
    src/gui.cpp
    src/gui_layout.cpp
    src/scrollbar.cpp
    src/widgets.cpp
    src/button.cpp
    src/combobox.cpp
    src/hotkey_widget.cpp
    src/popup_menu.cpp
    src/textbox.cpp
    src/listbox.cpp
    src/color_picker.cpp
    src/multiselect.cpp
    src/menu_tree.cpp
    src/menu_render.cpp
    src/font_cache.cpp
    src/font_metrics.cpp
    src/text_metrics.cpp
    src/render_queue.cpp
    src/math.cpp
    src/hotkey.cpp
    src/render/backend.cpp
    src/render/backend_circles.cpp
    src/render/backend_container.cpp
    src/render/backend_device.cpp
    src/render/backend_text.cpp
    src/render/font_runtime.cpp
    src/render/menu_background.cpp
    src/render/menu_window_assets.cpp
    src/menu/window_render.cpp
    src/menu/window_events.cpp
    src/menu/window_layout.cpp
    src/menu/window_lifetime.cpp
    src/menu/window_input.cpp
)

target_include_directories(skeet_menu_standalone PRIVATE
    include
)

target_link_libraries(skeet_menu_standalone PRIVATE
    d3d9
    user32
    gdi32
)

if(MSVC)
    target_compile_options(skeet_menu_standalone PRIVATE /W3 /permissive- /Zc:__cplusplus /utf-8)
endif()
