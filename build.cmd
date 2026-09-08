@echo off
setlocal

if not exist build mkdir build

cl /nologo /std:c17 /W4 /WX /MT /O2 ^
  /I includes ^
  src\main.c ^
  src\stn_stratum_server.c ^
  src\stn_rpc_client.c ^
  src\stn_rpc_win32.c ^
  /Fe:build\stn-stratum.exe ^
  /link Ws2_32.lib

if errorlevel 1 exit /b 1

echo.
echo Built: build\stn-stratum.exe