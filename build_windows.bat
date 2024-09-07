@echo off
setlocal

REM set directory to location of this file. Needed when drag n dropping across directories.
cd /d %~dp0

set ISO="%~1"

if %ISO%=="" (
    echo ERROR: iso path not passed
    echo USAGE: build_windows.bat vanilla_melee.iso
    goto end
) else (
    echo iso: %ISO%
)

if not exist %ISO% (
    echo ERROR: iso does not exist
    echo USAGE: build_windows.bat vanilla_melee.iso
    goto end
)

if not exist "C:/devkitPro/devkitPPC" (
    echo ERROR: devkitPro not found at "C:/devkitPro/devkitPPC"
    echo Please install devkitPro with the GameCube package
    goto cleanup
) else (
    echo found devkitPro
)

echo EXTRACT ISO FILES -----------------------------------------------
gc_fst extract %ISO% || ( echo ERROR: ISO Extraction failed. & goto cleanup )

echo MAKE SPACE IN ISO -----------------------------------------------
del "root\MvHowto.mth" "root\MvOmake15.mth" "root\MvOpen.mth"

echo BUILD C FILES --------------------------------------------------------

mkdir "root\TM\"

echo build patch\tmdata
copy "patch\tmdata\assets\evMenu.dat" "root\TM\TmDt.dat"
"MexTK/MexTK.exe" -ff -i "patch/tmdata/source/events.c" -b "build" -s tmFunction -dat "root/TM/TmDt.dat" -t "MexTK/tmFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'events.c' & goto cleanup )
"MexTK/MexTK.exe" -trim "root/TM/TmDt.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build patch\events\lab
copy "patch\events\lab\assets\labData.dat" "root\TM\EvLab.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab.c" -b "build" -s evFunction -dat "root/TM/EvLab.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'lab.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/EvLab.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build patch\events\lab_css
copy "patch\events\lab\assets\importData.dat" "root\TM\EvLabCSS.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab_css.c" -b "build" -s cssFunction -dat "root/TM/EvLabCSS.dat" -t "MexTK/cssFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'lab_css.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/EvLabCSS.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build patch\events\lcancel
copy "patch\events\lcancel\assets\lclData.dat" "root\TM\EvLCl.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/lcancel/source/lcancel.c" -b "build" -s evFunction -dat "root/TM/EvLCl.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'lcancel.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/EvLCl.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build patch\events\ledgedash
copy "patch\events\ledgedash\assets\ldshData.dat" "root\TM\EvLdsh.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/ledgedash/source/ledgedash.c" -b "build" -s evFunction -dat "root/TM/EvLdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'ledgedash.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/EvLdsh.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build patch\events\wavedash
copy "patch\events\wavedash\assets\wdshData.dat" "root\TM\EvWdsh.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/wavedash/source/wavedash.c" -b "build" -s evFunction -dat "root/TM/EvWdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'wavedash.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/EvLdsh.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo BUILD ASM FILES --------------------------------------------------------

cd "Build TM Codeset"
echo gecko.exe build
gecko.exe build || (
    echo ERROR: gecko.exe build failed
    cd ..
    goto cleanup
)
cd ..

echo BUILD START.DOL --------------------------------------------------------

copy "root\&&systemdata\Start.dol" "Start.dol"
del "root\&&systemdata\Start.dol"
"Build TM Start.dol\xdelta.exe" -d -f -s "Start.dol" "Build TM Start.dol\patch.xdelta" "root\&&systemdata\Start.dol" || ( echo ERROR: Failed to patch Start.dol & goto cleanup )
del "Start.dol"

echo COPYING EXTRA FILES ----------------------------------------------------

xcopy /s /e /y "Additional ISO Files\codes.gct" "root\"
xcopy /s /e /y "Additional ISO Files\*.mth" "root\"

echo REBUILD ISO --------------------------------------------------------
gc_fst rebuild root "TM-MORE.iso" || ( echo ERROR: ISO rebuild failed. & goto cleanup )

echo ############ TM-More.iso has been created ######################

:cleanup

echo CLEANUP -----------------------------------------------------------------

if exist "root" (
    echo deleting extracted files...
    rmdir /s /q root\
)

if exist "build" (
    echo deleting build dir...
    rmdir /s /q build\
)

:end

REM pause if not run from command line
echo %CMDCMDLINE% | findstr /C:"/c">nul && pause
