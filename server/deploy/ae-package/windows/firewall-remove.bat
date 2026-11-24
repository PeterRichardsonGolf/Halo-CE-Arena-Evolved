@echo off
net session >nul 2>&1 || (echo Run this as administrator. & exit /b 1)
netsh advfirewall firewall delete rule name="Arena Evolved server internet (UDP 2302)"
netsh advfirewall firewall delete rule name="Arena Evolved server LAN UDP"
netsh advfirewall firewall delete rule name="Arena Evolved server LAN map download (TCP 5153)"
