@echo off
rem Arena Evolved server: start it when Windows starts, with no one logged in (a scheduled task).
rem Run as administrator. The task runs as SYSTEM, which has no Steam of its own: put HALO_MCC_PATH in server-settings.bat
rem (start-server.bat explains the line) before you install it. SYSTEM also cannot see mapped network drives.
rem Why a task and not a Windows service: halo.exe does not speak the service protocol (sc create would time out with
rem error 1053), and a task needs nothing else installed.
setlocal
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
net session >nul 2>&1 || (echo Run this as administrator. & exit /b 1)
if not exist "%HERE%\server-settings.bat" echo Warning: no server-settings.bat, so the task will find MCC only if SYSTEM can see Steam. It cannot; add HALO_MCC_PATH first.
schtasks /create /tn "Arena Evolved server" /tr "\"%HERE%\run-task.bat\"" /sc onstart /ru SYSTEM /rl HIGHEST /f
echo Start it now with:  schtasks /run /tn "Arena Evolved server"
echo Stop it with:       schtasks /end /tn "Arena Evolved server"   (a hard stop; the web page's STOP is gentler)
echo Remove it with:     schtasks /delete /tn "Arena Evolved server" /f
