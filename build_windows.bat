@echo off

echo SANITY CHECKS -------------------------------------------------------------

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

if not exist "root" (
    mkdir "root"
)

if not exist "root\TM" (
    mkdir "root\TM"
)

if not exist "C:/devkitPro/devkitPPC" (
    echo ERROR: devkitPro not found at "C:/devkitPro/devkitPPC"
    echo Please install devkitPro with the GameCube package
    goto end
) else (
    echo found devkitPro
)

echo EXTRACTING ISO FILES -----------------------------------------------
gcrv1.3.exe --extract %ISO% .

echo BUILD C FILES --------------------------------------------------------

echo build patch\tmdata
copy "patch\tmdata\assets\evMenu.dat" "root\TM\TmDt.dat"
"MexTK/MexTK.exe" -ff -i "patch/tmdata/source/events.c" -b "build" -s tmFunction -dat "root/TM/TmDt.dat" -t "MexTK/tmFunction.txt" -q -ow -l "MexTK/melee.link" -op 1


echo build patch\events\lab
copy "patch\events\lab\assets\labData.dat" "root\TM\EvLab.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab.c" -b "build" -s evFunction -dat "root/TM/EvLab.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1

copy "patch\events\lab\assets\importData.dat" "root\TM\EvLabCSS.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab_css.c" -b "build" -s cssFunction -dat "root/TM/EvLabCSS.dat" -t "MexTK/cssFunction.txt" -q -ow -l "MexTK/melee.link" -op 1


echo build patch\events\lcancel
copy "patch\events\lcancel\assets\lclData.dat" "root\TM\EvLCl.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/lcancel/source/lcancel.c" -b "build" -s evFunction -dat "root/TM/EvLCl.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1


echo build patch\events\ledgedash
copy "patch\events\ledgedash\assets\ldshData.dat" "root\TM\EvLdsh.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/ledgedash/source/ledgedash.c" -b "build" -s evFunction -dat "root/TM/EvLdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1


echo build patch\events\wavedash
copy "patch\events\wavedash\assets\wdshData.dat" "root\TM\EvWdsh.dat"
"MexTK/MexTK.exe" -ff -i "patch/events/wavedash/source/wavedash.c" -b "build" -s evFunction -dat "root/TM/EvWdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1


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
"Build TM Start.dol\xdelta.exe" -d -f -s "Start.dol" "Build TM Start.dol\patch.xdelta" "root\&&systemdata\Start.dol"
del "Start.dol"

echo COPYING EXTRA FILES ----------------------------------------------------

xcopy /s /e /y "Additional ISO File\" "root\"

echo REBUILD ISO --------------------------------------------------------
gcrv1.3.exe --rebuild root "TM-MORE.iso" idk_what_this_arg_does_but_gcr_crashes_without_it

echo ############ TM-More.iso has been successfully created ######################

:cleanup

echo CLEANUP -----------------------------------------------------------------

if exist "root" (
    echo deleting root\...
    del /s /q root > nul
    rmdir root
)

if exist "build" (
    del /s /q build
    rmdir build
)

:end
