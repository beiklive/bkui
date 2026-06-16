@echo off
setlocal
set "PATH=E:\bin\msys64\mingw64\bin;%PATH%"
cmake -S . -B build\windows -G "MinGW Makefiles" -DBKUI_SHARED=OFF
cmake --build build\windows
