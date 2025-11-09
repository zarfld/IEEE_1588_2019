@echo off
setlocal enableextensions
set SCRIPT_DIR=%~dp0
set PY_SCRIPT=%SCRIPT_DIR%generate_traceability_matrix.py

rem Try Python launcher (3.x preferred), then fallback to py, then python
py -3 "%PY_SCRIPT%" %*
if %errorlevel% equ 0 goto :eof

py "%PY_SCRIPT%" %*
if %errorlevel% equ 0 goto :eof

python "%PY_SCRIPT%" %*
exit /b %errorlevel%
