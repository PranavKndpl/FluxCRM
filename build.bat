@echo off
echo Building FluxCRM...

g++ src/main.cpp ^
    vendor/imgui/imgui.cpp ^
    vendor/imgui/imgui_draw.cpp ^
    vendor/imgui/imgui_tables.cpp ^
    vendor/imgui/imgui_widgets.cpp ^
    vendor/imgui/backends/imgui_impl_win32.cpp ^
    vendor/imgui/backends/imgui_impl_dx11.cpp ^
    -o flux_crm.exe ^
    -Ivendor/imgui ^
    -Ivendor/imgui/backends ^
    -Ivendor/fluxdb ^
    -ld3d11 -ld3dcompiler -lws2_32 -lgdi32 -ldwmapi ^
    -static -O3

if %errorlevel% neq 0 (
    echo Build Failed!
) else (
    echo Build Success! Run flux_crm.exe
)