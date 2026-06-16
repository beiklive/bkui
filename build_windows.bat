@echo off
setlocal
set "PATH=E:\bin\msys64\mingw64\bin;%PATH%"
for /f %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "[DateTimeOffset]::Now.ToUnixTimeMilliseconds()"') do set "BUILD_START_MS=%%i"
cmake -S . -B build\windows -G "MinGW Makefiles" -DBKUI_SHARED=OFF -DBKUI_PLATFORM_SWITCH=OFF
if errorlevel 1 exit /b %errorlevel%
cmake --build build\windows --parallel
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$start = [DateTimeOffset]::FromUnixTimeMilliseconds([int64]$env:BUILD_START_MS).LocalDateTime;" ^
  "$end = Get-Date;" ^
  "$elapsed = New-TimeSpan -Start $start -End $end;" ^
  "$artifacts = Get-ChildItem -Path 'build/windows/bin' -File -Filter '*.exe' -ErrorAction SilentlyContinue | Sort-Object Name;" ^
  "Write-Host '';" ^
  "Write-Host 'Build summary';" ^
  "Write-Host ('  Start:   {0:yyyy-MM-dd HH:mm:ss}' -f $start);" ^
  "Write-Host ('  End:     {0:yyyy-MM-dd HH:mm:ss}' -f $end);" ^
  "Write-Host ('  Elapsed: {0:mm\:ss\.fff}' -f $elapsed);" ^
  "if ($artifacts.Count -eq 0) { Write-Host '  No executable artifacts found in build/windows/bin'; }" ^
  "foreach ($file in $artifacts) { Write-Host ('  {0}: {1:N0} bytes ({2:N2} MiB)' -f $file.FullName, $file.Length, ($file.Length / 1MB)); }"
