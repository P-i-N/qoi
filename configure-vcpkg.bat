@echo off
set TRIPLET=x64-windows-static

set LIBRARY=libpng
vcpkg list | findstr /C:"%LIBRARY%:%TRIPLET%"
if errorlevel 1 vcpkg install %LIBRARY% --triplet %TRIPLET%
