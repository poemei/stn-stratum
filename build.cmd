@echo off
setlocal

if not exist build mkdir build

if /i "%~1"=="test-miner-job" goto test_miner_job

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
    src\stn_miner_job.c ^
    platforms\windows\stn_rpc_win32.c ^
    /Fe:build\stn-stratum.exe ^
    /link Ws2_32.lib

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

goto build_success

:test_miner_job
where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: Microsoft cl.exe not found.
    echo Run this from a Visual Studio Developer Command Prompt.
    exit /b 1
)
cl /nologo /O2 /W4 /std:c17 /Iincludes /DSTN_MINER_JOB_TEST_MAIN ^
    tests\test_miner_job.c ^
    src\stn_miner_job.c ^
    /Fe:build\test-miner-job.exe
if errorlevel 1 exit /b 1
build\test-miner-job.exe
exit /b %errorlevel%

:build_success
echo.
echo BUILD SUCCESSFUL
echo build\stn-stratum.exe
exit /b 0
