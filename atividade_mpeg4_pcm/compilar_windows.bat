@echo off
setlocal

rem Execute no x64 Native Tools Command Prompt do Visual Studio.
rem Ajuste GST_DIR se o GStreamer foi instalado em outra pasta.
if not defined GST_DIR set "GST_DIR=C:\gstreamer\1.0\msvc_x86_64"

where cl >nul 2>nul
if errorlevel 1 (
    echo Abra o x64 Native Tools Command Prompt do Visual Studio e tente novamente.
    exit /b 1
)
if not exist "%GST_DIR%\include\gstreamer-1.0\gst\gst.h" (
    echo Instale o GStreamer Development MSVC x86_64 ou ajuste GST_DIR.
    exit /b 1
)

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
