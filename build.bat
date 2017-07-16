 @echo off
 
 set CommonCompilerFlags=-nologo -MTd -fp:fast -Gm- -GR- -EHa -WX -Oi -W4 -FC -wd4010 -wd4201 -wd4204 -IE:\Documents\Coding\C\h -DINTERNAL=1 -DDEBUG_PREFIX=Main
 set DebugCompilerFlags=-Od -Z7
 set CommonLinkerFlags=-incremental:no -opt:ref user32.lib gdi32.lib winmm.lib
 REM user32.lib gdi32.lib
 
 IF NOT EXIST E:\Documents\Coding\C\build mkdir E:\Documents\Coding\C\build
 pushd E:\Documents\Coding\C\build
 
 call E:\Documents\Coding\C\shell64.bat
 
 echo WAITING FOR PDB > lock.tmp
 
 timethis cl %CommonCompilerFlags% %DebugCompilerFlags% E:\Documents\Coding\C\geometer\geometer.c -Fmgeometer.map -LD /link %CommonLinkerFlags% -PDB:geometer_%random%.pdb -EXPORT:UpdateAndRender
 del lock.tmp
 
 timethis cl %CommonCompilerFlags% %DebugCompilerFlags% E:\Documents\Coding\C\geometer\win32_geometer.c -Fmwin32_geometer.map /link %CommonLinkerFlags%
 
 echo Finished at %time%
 REM ./win32_geometer.exe
 popd
 
 
 REM Flag Meanings:
 REM ==============
 REM
 REM -nologo	- no Microsoft logo at the beginning of compilation
 REM -Od		- no optimisation of code at all
 REM -Oi		- use intrinsic version of function if exists
 REM -Z7		- compatible debug info for debugger (replaced -Zi)
 REM -GR-		- turn off runtime type info (C++)
 REM -EHa-		- turn off exception handling (C++)
 REM -W4		- 4th level of warnings
 REM -WX 		- treat warnings as errors
 REM -wd#### 	- remove warning ####
 REM -D#####	- #define #### (=1)
 REM -Gm-		- turn off 'minimal rebuild' - no incremental build
 REM -Fm####	- provides location for compiler to put a .map file
 REM -I####		- search for include files at ####
 REM
 REM -MTd		- use (d => debug version of) static CRT library - needed for running on XP
 REM /link -subsystem:windows,5.1 - ONLY FOR 32-BIT BUILDS!!! - needed for running on XP
 
 REM Warnings Removed:
 REM =================
 REM
 REM C4201: nonstandard extension used: nameless struct/union
 REM C4100: unreferenced formal parameter
 REM C4189: local variable is initialized but not referenced
 REM C4204: nonstandard extension used: non-constant aggregate initializer
