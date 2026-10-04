@echo off
cd /d "%~dp0"
Lithtech.exe -rez Engine.REZ -rez rez -config autoexec.cfg +runworld "Worlds/World" +autostart 1
