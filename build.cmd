@echo off
setlocal

if not exist build mkdir build

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: Microsoft cl.exe not found.
    echo Run this from a Visual Studio Developer Command Prompt.
    exit /b 1
)

cl /nologo /O2 /W4 /std:c17 /Iincludes /Iplatforms/windows ^
    src\main.c ^
    src\stn_stratum_server.c ^
    src\stn_rpc_client.c ^
    src\stn_chain_config.c ^
    src\stn_share_verify.c ^
    platforms\windows\stn_rpc_win32.c ^
    /Fe:build\stn-stratum.exe ^
    /link Ws2_32.lib

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo BUILD SUCCESSFUL
echo build\stn-stratum.exe
exit /b 0
