@echo off
rem What the "Arena Evolved server" startup task runs (install-task.bat): start-server.bat without the pause at the end.
set "AE_SERVER_NO_PAUSE=1"
call "%~dp0start-server.bat"
