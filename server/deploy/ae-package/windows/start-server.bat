@echo off
rem Arena Evolved dedicated server for Windows. Double-click it, or run it from a command prompt: Ctrl+C stops it.
rem Folders: data\ (the game's data root: packs\ holds Arena Evolved's weapon data), save\ (settings, logs, saves).
rem The rotation is read from rotation.txt on every start: one line, entries separated by commas (lines starting with # are notes).
rem MCC is found through Steam on its own. If it is not (a second drive, a service running as another user), put the line
rem     set "HALO_MCC_PATH=D:\SteamLibrary\steamapps\common\Halo The Master Chief Collection"
rem in server-settings.bat beside this file.
setlocal
set "HERE=%~dp0"
if "%HERE:~-1%"=="\" set "HERE=%HERE:~0,-1%"
if not exist "%HERE%\data" mkdir "%HERE%\data"
if not exist "%HERE%\data\maps" mkdir "%HERE%\data\maps"
if not exist "%HERE%\save" mkdir "%HERE%\save"
set "HALO_DATA_ROOT=%HERE%\data"
set "HALO_SAVE_ROOT=%HERE%\save"
if not defined HALO_SERVER_ROTATION (
  for /f "usebackq delims=" %%R in (`powershell -NoProfile -Command "((Get-Content -LiteralPath '%HERE%\rotation.txt') | Where-Object { $_.Trim() -ne '' -and -not $_.TrimStart().StartsWith('#') } | ForEach-Object { $_.Trim().Trim(',') }) -join ','"`) do set "HALO_SERVER_ROTATION=%%R"
)
set "HALO_NET_TUNNEL_PORT=2302"
set "SDL_VIDEODRIVER=dummy"
set "SDL_AUDIODRIVER=dummy"
set "SDL_AUDIO_DRIVER=dummy"
rem Several servers on one PC (GUIDE.md): server-settings.bat can set HALO_NET_ADDRESS, HALO_SERVER_WEB_PORT, HALO_NET_TUNNEL_PORT.
rem Bots (GUIDE.md, "Bots"): HALO_SERVER_MIN_PLAYERS, HALO_SERVER_BOTS_FILL, HALO_SERVER_BOTS_SKILL, HALO_SERVER_BOTS_ALONE.
if exist "%HERE%\server-settings.bat" call "%HERE%\server-settings.bat"
cd /d "%HERE%\data"
rem (halo.exe has no console of its own; start /wait /b makes this window show its log and wait for it)
start "" /wait /b "%HERE%\halo.exe" --server %*
echo.
echo The server stopped (exit code %ERRORLEVEL%). Its log is %HERE%\data\debug.txt
if not "%AE_SERVER_NO_PAUSE%"=="1" pause
