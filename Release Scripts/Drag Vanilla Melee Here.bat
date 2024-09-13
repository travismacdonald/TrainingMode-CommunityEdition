@echo off
setlocal

REM set directory to location of this file. Needed when drag n dropping across directories.
cd /d %~dp0

set ISO="%~1"

if %ISO%=="" (
    echo Please drag a vanilla melee iso on this script
    goto end
)

if not exist %ISO% (
    echo Please drag a vanilla melee iso on this script
    goto end
)

echo Building TM-More.iso...

gc_fst.exe extract %ISO%
del "root\MvHowto.mth" "root\MvOmake15.mth" "root\MvOpen.mth"
xcopy /s /e /y patch root
gc_fst rebuild root "TM-More.iso" 
rmdir /s /q root

echo ############ TM-More.iso has been created ######################

:end
