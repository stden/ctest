@echo off
echo == AStyle ==
AStyle --options=astyle.cfg *.c
AStyle --options=astyle.cfg *.h 

del *.bak
del *.orig 

