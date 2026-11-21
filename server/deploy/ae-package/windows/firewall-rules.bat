@echo off
rem Arena Evolved server: Windows Firewall rules. Run as administrator (right-click, Run as administrator).
rem Internet play needs only UDP 2302. The game's LAN ports (UDP 5150, 5151, 5160, TCP 5153) are opened for the local
rem subnet only. The web page (TCP 8080) stays on this PC's loopback and needs no rule. Remove the rules with firewall-remove.bat.
setlocal
set "HERE=%~dp0"
net session >nul 2>&1 || (echo Run this as administrator. & exit /b 1)
set "PROG=%HERE%halo.exe"
netsh advfirewall firewall add rule name="Arena Evolved server internet (UDP 2302)" dir=in action=allow protocol=UDP localport=2302 program="%PROG%" profile=any
netsh advfirewall firewall add rule name="Arena Evolved server LAN UDP" dir=in action=allow protocol=UDP localport=5150,5151,5160 remoteip=LocalSubnet program="%PROG%" profile=private,domain
netsh advfirewall firewall add rule name="Arena Evolved server LAN map download (TCP 5153)" dir=in action=allow protocol=TCP localport=5153 remoteip=LocalSubnet program="%PROG%" profile=private,domain
echo Done. Windows may still ask once when the server first starts: allow it for the networks you chose above.
