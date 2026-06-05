@echo off

set "VS_VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "VS_ARCH=x64"

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build"

set "TARGET=handmade-hero"
set "SOURCE=..\src\win32-handmade.cpp"

set "CL_FLAGS=-FS -FC -Zi "
set "CL_OUTPUT=-Fe:%TARGET%.exe -Fd:%TARGET%.pdb -Fo:%TARGET%.obj"
set "LIBS=user32.lib Gdi32.lib "

call "%VS_VCVARS%" %VS_ARCH% >nul

mkdir "%BUILD_DIR%" 2>nul
pushd "%BUILD_DIR%"
cl %CL_FLAGS% %CL_OUTPUT% %SOURCE% %LIBS%
popd
