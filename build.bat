@echo off
if not exist "bin" mkdir bin
if not exist "obj" mkdir obj

:: Check if ImGui AND ImPlot are compiled
if exist "obj\imgui.o" if exist "obj\implot.o" goto :COMPILE_MAIN

echo [1/2] Compiling GUI Libraries (ImGui + ImPlot)...
g++ -c -O3 ^
    vendor/imgui/imgui.cpp ^
    vendor/imgui/imgui_draw.cpp ^
    vendor/imgui/imgui_tables.cpp ^
    vendor/imgui/imgui_widgets.cpp ^
    vendor/imgui/backends/imgui_impl_win32.cpp ^
    vendor/imgui/backends/imgui_impl_dx11.cpp ^
    vendor/implot/implot.cpp ^
    vendor/implot/implot_items.cpp ^
    -Ivendor/imgui -Ivendor/imgui/backends -Ivendor/implot

move *.o obj\ > nul
echo Libs Compiled!

:COMPILE_MAIN
echo [2/2] Compiling FluxCRM App...

g++ src/main.cpp obj/*.o ^
    -o bin/flux_crm.exe ^
    -Ivendor/imgui ^
    -Ivendor/imgui/backends ^
    -Ivendor/fluxdb ^
    -Ivendor/implot ^
    -ld3d11 -ld3dcompiler -lws2_32 -lgdi32 -ldwmapi ^
    -static -O3

if %errorlevel% neq 0 (
    echo Build Failed!
) else (
    echo Build Success! (Run bin\flux_crm.exe)
)