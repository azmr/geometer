@echo off

set SingleExecutable=0
set Warnings=-WX -W4 -wd4010 -wd4201 -wd4204 -wd4146
set CommonOptimisations=-fp:fast -Oi -Gm- -GR- -EHa
set Defines=-DINTERNAL=1 -DDEBUG_PREFIX=Main -DSINGLE_EXECUTABLE=%SingleExecutable%
set Includes=-IE:\Documents\Coding\C\h
set CommonCompilerFlags=-nologo -MTd -FC %Warnings% %CommonOptimisations% %Defines% %Includes%
set DebugCompilerFlags=%1 -Z7
set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib Comdlg32.lib

call E:\Documents\Coding\C\shell64.bat

REM IF NOT EXIST E:\Documents\Coding\C\geometer\geometer.res
rc -nologo E:\Documents\Coding\C\geometer\geometer.rc
IF NOT EXIST E:\Documents\Coding\C\build mkdir E:\Documents\Coding\C\build
pushd E:\Documents\Coding\C\build

@del *geometer*.pdb > nul 2>&1

echo WAITING FOR PDB > lock.tmp

IF %SingleExecutable%==0 timethis cl %CommonCompilerFlags% %DebugCompilerFlags% E:\Documents\Coding\C\geometer\geometer.c -Fmgeometer.map -LD /link %CommonLinkerFlags% -PDB:geometer_%random%.pdb -EXPORT:UpdateAndRender
del lock.tmp

set CommonCompilerFlags=%CommonCompilerFlags% 
timethis cl %CommonCompilerFlags% %DebugCompilerFlags% E:\Documents\Coding\C\geometer\win32_geometer.c -Fmwin32_geometer.map /link %CommonLinkerFlags% E:\Documents\Coding\C\geometer\geometer.res

start "Geometer tests" cmd /c "timethis cl -nologo -MTd -FC %CommonOptimisations% %Defines% %Includes% %DebugCompilerFlags% E:\Documents\Coding\C\geometer\test_win32_geometer.c /link -PDB:test_win32_geometer.pdb -SUBSYSTEM:CONSOLE %CommonLinkerFlags% && test_win32_geometer.exe & pause"

REM start "Macro expansion" cmd /c "timethis cl -nologo -MTd -FC %CommonOptimisations% %Defines% %Includes% %DebugCompilerFlags% E:\Documents\Coding\C\geometer\test_win32_geometer.c -E /link -PDB:test_win32_geometer.pdb -SUBSYSTEM:CONSOLE %CommonLinkerFlags% > E:\Documents\Coding\C\geometer\test_win32_geometer.i & pause"

echo Finished at %time%
REM ./win32_geometer.exe
popd


REM Flag Meanings:
REM ==============
REM
REM -nologo	- no Microsoft logo at the beginning of compilation
REM -Od		- no optimisation of code at all
REM -O2		- optimisation for speed
REM -O3		- optimisation for space
REM -Oi		- use intrinsic version of function if exists
REM -Z7		- compatible debug info for debugger (replaced -Zi)
REM -GR-	- turn off runtime type info (C++)
REM -EHa-	- turn off exception handling (C++)
REM -W4		- 4th level of warnings
REM -WX 	- treat warnings as errors
REM -wd#### - remove warning ####
REM -D#####	- #define #### (=1)
REM -Gm-	- turn off 'minimal rebuild' - no incremental build
REM -Fm####	- provides location for compiler to put a .map file
REM -I####	- search for include files at ####
REM -FC		- display full path of source code
REM
REM -MTd	- use (d => debug version of) static CRT library - needed for running on XP
REM /link -subsystem:windows,5.1 - ONLY FOR 32-BIT BUILDS!!! - needed for running on XP

REM Warnings Removed:
REM =================
REM
REM C4201: nonstandard extension used: nameless struct/union
REM C4100: unreferenced formal parameter
REM C4189: local variable is initialized but not referenced
REM C4204: nonstandard extension used: non-constant aggregate initializer
REM C4146: unary minus operator applied to unsigned type, result still unsigned
