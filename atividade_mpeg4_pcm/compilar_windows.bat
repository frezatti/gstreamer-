@echo off
setlocal

rem Execute no x64 Native Tools Command Prompt do Visual Studio.
rem GST_DIR pode indicar uma instalacao personalizada.

where cl >nul 2>nul
if errorlevel 1 (
    echo Abra o x64 Native Tools Command Prompt do Visual Studio e tente novamente.
    exit /b 1
)
if defined GST_DIR if exist "%GST_DIR%\include\gstreamer-1.0\gst\gst.h" goto compilar
if exist "%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\include\gstreamer-1.0\gst\gst.h" (
    set "GST_DIR=%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64"
    goto compilar
)
if exist "%ProgramFiles%\gstreamer\1.0\msvc_x86_64\include\gstreamer-1.0\gst\gst.h" (
    set "GST_DIR=%ProgramFiles%\gstreamer\1.0\msvc_x86_64"
    goto compilar
)
if exist "C:\gstreamer\1.0\msvc_x86_64\include\gstreamer-1.0\gst\gst.h" (
    set "GST_DIR=C:\gstreamer\1.0\msvc_x86_64"
    goto compilar
)
echo Nao foi encontrado include\gstreamer-1.0\gst\gst.h.
echo GST_DIR informado: "%GST_DIR%"
echo Instale GStreamer MSVC x86_64 com Runtime and development headers.
echo Se instalou em outra pasta, defina GST_DIR para essa pasta, sem \bin.
exit /b 1

:compilar
echo GStreamer: "%GST_DIR%"
cl /nologo /std:c11 /W4 /MD "%~dp0main.c" /Fe:atividade.exe ^
    /I"%GST_DIR%\include\gstreamer-1.0" ^
    /I"%GST_DIR%\include\glib-2.0" ^
    /I"%GST_DIR%\lib\glib-2.0\include" ^
    /link /LIBPATH:"%GST_DIR%\lib" ^
    gstreamer-1.0.lib gobject-2.0.lib glib-2.0.lib

if errorlevel 1 exit /b 1
echo Compilado: atividade.exe
echo Antes de executar, adicione %GST_DIR%\bin ao PATH desta janela.
endlocal
