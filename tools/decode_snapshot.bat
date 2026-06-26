@echo off
REM Decode MCU Malloc Tracker binary snapshot (Windows)
REM Usage: decode_snapshot.bat snapshot.bin [--filemap filemap.txt]

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "PYTHON_SCRIPT=%SCRIPT_DIR%mt_decode.py"

if not exist "%PYTHON_SCRIPT%" (
    echo Error: mt_decode.py not found at %PYTHON_SCRIPT%
    exit /b 1
)

if "%1"=="" (
    echo Usage: %0 ^<snapshot.bin^> [--filemap ^<filemap.txt^>] [--json ^<output.json^>] [--top N]
    echo.
    echo Example:
    echo   %0 snapshot_dump.bin
    echo   %0 snapshot_dump.bin --filemap filemap.txt
    exit /b 1
)

where python3 >nul 2>nul && (
    python3 "%PYTHON_SCRIPT%" %*
    exit /b %errorlevel%
)

where python >nul 2>nul && (
    python "%PYTHON_SCRIPT%" %*
    exit /b %errorlevel%
)

where py >nul 2>nul && (
    py "%PYTHON_SCRIPT%" %*
    exit /b %errorlevel%
)

echo Error: python3, python, or py was not found in PATH.
exit /b 1
