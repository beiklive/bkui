@echo off
setlocal
set "PATH=E:\bin\msys64\mingw64\bin;%PATH%"
cmake -S . -B build\windows -G "MinGW Makefiles" -DBKUI_SHARED=OFF -DBKUI_PLATFORM_SWITCH=OFF
cmake --build build\windows --parallel
