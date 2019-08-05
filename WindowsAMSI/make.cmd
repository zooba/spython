@setlocal
@echo off
if not defined PYTHONDIR echo PYTHONDIR must be set before building && exit /B 1
if exist "%PYTHONDIR%\PCbuild" (
    set _PYTHONINCLUDE=-I"%PYTHONDIR%\PC" -I"%PYTHONDIR%\include"
    if "%VSCMD_ARG_TGT_ARCH%" == "x86" (
        set _PYTHONLIB=%PYTHONDIR%\PCbuild\win32
    ) else (
        set _PYTHONLIB=%PYTHONDIR%\PCbuild\amd64
    )
) else (
    set _PYTHONINCLUDE=-I"%PYTHONDIR%\include"
    set _PYTHONLIB=%PYTHONDIR%\libs
)

if exist "%_PYTHONLIB%\python_d.exe" (
    set _MD=-MDd
) else (
    set _MD=-MD
)

@echo on
@if not exist obj mkdir obj
cl -nologo %_MD% -c spython.c -Foobj\spython.obj -Iobj -Zi -O2 %_PYTHONINCLUDE%
@if errorlevel 1 exit /B %ERRORLEVEL%
link /nologo obj\spython.obj amsi.lib /out:spython.exe /debug:FULL /pdb:spython.pdb /libpath:"%_PYTHONLIB%"
@if errorlevel 1 exit /B %ERRORLEVEL%
