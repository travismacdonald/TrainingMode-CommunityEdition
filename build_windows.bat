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
gc_fst set-header "root/&&systemdata/ISO.hdr" "GTME01" "Melee Training Mode - More"

echo MAKE SPACE IN ISO -----------------------------------------------
del "root\MvHowto.mth" "root\MvOmake15.mth" "root\MvOpen.mth"

echo BUILD C FILES --------------------------------------------------------

mkdir "root\TM\"

echo build event menu
copy "dats\eventMenu.dat" "root\TM\eventMenu.dat"
"MexTK/MexTK.exe" -ff -i "src/events.c" -b "build" -s tmFunction -dat "root/TM/eventMenu.dat" -t "MexTK/tmFunction.txt" -q -ow -l "MexTK/melee.link" -op 2 || ( echo ERROR: Failed to compile 'events.c' & goto cleanup )
"MexTK/MexTK.exe" -trim "root/TM/eventMenu.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build lab event
copy "dats\lab.dat" "root\TM\lab.dat"
"MexTK/MexTK.exe" -ff -i "src/lab.c" -b "build" -s evFunction -dat "root/TM/lab.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 2 || ( echo ERROR: Failed to compile 'lab.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/lab.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build lab css
copy "dats\labCSS.dat" "root\TM\labCSS.dat"
"MexTK/MexTK.exe" -ff -i "src/lab_css.c" -b "build" -s cssFunction -dat "root/TM/labCSS.dat" -t "MexTK/cssFunction.txt" -q -ow -l "MexTK/melee.link" -op 2 || ( echo ERROR: Failed to compile 'lab_css.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/labCSS.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build lcancel event
copy "dats\lcancel.dat" "root\TM\lcancel.dat"
"MexTK/MexTK.exe" -ff -i "src/lcancel.c" -b "build" -s evFunction -dat "root/TM/lcancel.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 2 || ( echo ERROR: Failed to compile 'lcancel.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/lcancel.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build ledgedash event
copy "dats\ledgedash.dat" "root\TM\ledgedash.dat"
"MexTK/MexTK.exe" -ff -i "src/ledgedash.c" -b "build" -s evFunction -dat "root/TM/ledgedash.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 2 || ( echo ERROR: Failed to compile 'ledgedash.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/ledgedash.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

echo build wavedash event
copy "dats\wavedash.dat" "root\TM\wavedash.dat"
"MexTK/MexTK.exe" -ff -i "src/wavedash.c" -b "build" -s evFunction -dat "root/TM/wavedash.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 2 || ( echo ERROR: Failed to compile 'wavedash.c' & goto cleanup)
"MexTK/MexTK.exe" -trim "root/TM/wavedash.dat" || ( echo ERROR: Dat file trimming failed & goto cleanup )

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
xcopy /s /e /y "Additional ISO Files\opening.bnr" "root\"
xcopy /s /e /y "Additional ISO Files\*.mth" "root\"

echo REBUILD ISO --------------------------------------------------------
gc_fst rebuild root "TM-More.iso" || ( echo ERROR: ISO rebuild failed. & goto cleanup )

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
