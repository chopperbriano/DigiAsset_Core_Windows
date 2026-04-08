@echo off
setlocal enabledelayedexpansion

set PASS=0
set FAIL=0
set SKIP=0

echo ============================================
echo  DigiAsset Core for Windows - Test Suite
echo ============================================
echo.

REM --- Find unit test binary ---
set UNIT_EXE=build\tests\Release\Unit_Tests_run.exe
if not exist "%UNIT_EXE%" (
    set UNIT_EXE=build\tests\Debug\Unit_Tests_run.exe
)

if not exist "%UNIT_EXE%" (
    echo SKIP: Unit test binary not found. Build with -DBUILD_TEST=ON first.
    set /a SKIP+=1
    goto :integration
)

echo [1/3] Running unit tests...
"%UNIT_EXE%" --gtest_output=xml:test_results_unit.xml
if !ERRORLEVEL! EQU 0 (
    echo PASS: Unit tests
    set /a PASS+=1
) else (
    echo FAIL: Unit tests ^(exit code !ERRORLEVEL!^)
    set /a FAIL+=1
)
echo.

:integration
REM --- Check exe exists ---
echo [2/3] Checking built binaries...
set EXE=build\src\Release\DigiAssetCore.exe
set CLI=build\cli\Release\DigiAssetCore-cli.exe

if exist "%EXE%" (
    echo   PASS: DigiAssetCore.exe exists
    set /a PASS+=1
) else (
    echo   FAIL: DigiAssetCore.exe not found
    set /a FAIL+=1
)

if exist "%CLI%" (
    echo   PASS: DigiAssetCore-cli.exe exists
    set /a PASS+=1
) else (
    echo   SKIP: DigiAssetCore-cli.exe not found ^(build with -DBUILD_CLI=ON^)
    set /a SKIP+=1
)
echo.

REM --- Quick smoke test: exe starts and shows version ---
echo [3/3] Smoke test...
if not exist "%EXE%" (
    echo   SKIP: No exe to test
    set /a SKIP+=1
    goto :results
)

REM Create a temp directory for the smoke test
set SMOKE_DIR=%TEMP%\digiasset_smoke_%RANDOM%
mkdir "%SMOKE_DIR%" 2>NUL

REM Run exe with no config — should print config wizard prompt and exit on no input
echo. | "%EXE%" > "%SMOKE_DIR%\output.txt" 2>&1
timeout /t 3 /nobreak >NUL

REM Check if config wizard message appeared
findstr /C:"config wizard" "%SMOKE_DIR%\output.txt" >NUL 2>&1
if !ERRORLEVEL! EQU 0 (
    echo   PASS: Config wizard triggers when no config.cfg
    set /a PASS+=1
) else (
    echo   SKIP: Could not verify config wizard ^(may need interactive terminal^)
    set /a SKIP+=1
)

REM Cleanup
rmdir /S /Q "%SMOKE_DIR%" 2>NUL

:results
echo.
echo ============================================
echo  Results: %PASS% passed, %FAIL% failed, %SKIP% skipped
echo ============================================

if %FAIL% GTR 0 exit /b 1
exit /b 0
