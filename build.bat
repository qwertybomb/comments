@echo off
cl.exe -nologo -Oi -O2 -GS- comments.c -link -subsystem:console -nodefaultlib kernel32.lib shell32.lib
