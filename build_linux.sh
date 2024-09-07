#!/usr/bin/env bash

if [ -z ${1} ]; then 
    echo ERROR: iso path not passed
    echo USAGE: build_linux.sh vanilla_melee.iso
    exit 1
fi

ISO=${1}

if ! [ -x "$(command -v powerpc-eabi-as)" ]; then
    echo ERROR: 'powerpc-eabi-as' was not found. Was devkitpro installed and /opt/devkitpro/devkitPPC/bin/ added to the PATH?
    exit 1
fi

if ! [ -x "$(command -v xdelta3)" ]; then
    echo ERROR: 'xdelta3' was not found. Was xdelta installed and added to the PATH?
    exit 1
fi

cleanup () {
    echo CLEANUP -----------------------------------------------------------------

    if [ -d "root" ]; then
        echo deleting extracted files...

        # terrifying...
        rm -rd ./root/
    fi

    if [ -d "build" ]; then
        echo deleting build dir...
        rm -rd ./build/
    fi

    exit $1
}

set -e
trap cleanup ERR

echo EXTRACT ISO FILES -----------------------------------------------
# mono gcr.exe --extract ${ISO} . || ( echo ERROR: ISO Extraction failed. Are mono and gcr.exe present? && exit 1 )
./gc_fst extract ${ISO}

echo MAKE SPACE IN ISO -----------------------------------------------
rm "root/MvHowto.mth" "root/MvOmake15.mth" "root/MvOpen.mth"

echo BUILD C FILES --------------------------------------------------------

mkdir root/TM/

echo build patch/tmdata
cp "patch/tmdata/assets/evMenu.dat" "root/TM/TmDt.dat"
mono "MexTK/MexTK.exe" -ff -i "patch/tmdata/source/events.c" -b "build" -s tmFunction -dat "root/TM/TmDt.dat" -t "MexTK/tmFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'events.c' && exit 1)
mono "MexTK/MexTK.exe" -trim "root/TM/TmDt.dat" || ( echo ERROR: Dat file trimming failed && exit 1 )

echo build patch/events/lab
cp "patch/events/lab/assets/labData.dat" "root/TM/EvLab.dat"
mono "MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab.c" -b "build" -s evFunction -dat "root/TM/EvLab.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'lab.c' && exit 1)
mono "MexTK/MexTK.exe" -trim "root/TM/EvLab.dat" || ( echo ERROR: Dat file trimming failed && exit 1 )

echo build patch/events/lab_css
cp "patch/events/lab/assets/importData.dat" "root/TM/EvLabCSS.dat"
mono "MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab_css.c" -b "build" -s cssFunction -dat "root/TM/EvLabCSS.dat" -t "MexTK/cssFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'lab_css.c' && exit 1)
mono "MexTK/MexTK.exe" -trim "root/TM/EvLabCSS.dat" || ( echo ERROR: Dat file trimming failed && exit 1 )

echo build patch/events/lcancel
cp "patch/events/lcancel/assets/lclData.dat" "root/TM/EvLCl.dat"
mono "MexTK/MexTK.exe" -ff -i "patch/events/lcancel/source/lcancel.c" -b "build" -s evFunction -dat "root/TM/EvLCl.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'lcancel.c' && exit 1)
mono "MexTK/MexTK.exe" -trim "root/TM/EvLCl.dat" || ( echo ERROR: Dat file trimming failed && exit 1 )

echo build patch/events/ledgedash
cp "patch/events/ledgedash/assets/ldshData.dat" "root/TM/EvLdsh.dat"
mono "MexTK/MexTK.exe" -ff -i "patch/events/ledgedash/source/ledgedash.c" -b "build" -s evFunction -dat "root/TM/EvLdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'ledgedash.c' && exit 1)
mono "MexTK/MexTK.exe" -trim "root/TM/EvLdsh.dat" || ( echo ERROR: Dat file trimming failed && exit 1 )

echo build patch/events/wavedash
cp "patch/events/wavedash/assets/wdshData.dat" "root/TM/EvWdsh.dat"
mono "MexTK/MexTK.exe" -ff -i "patch/events/wavedash/source/wavedash.c" -b "build" -s evFunction -dat "root/TM/EvWdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1 || ( echo ERROR: Failed to compile 'wavedash.c' && exit 1)
mono "MexTK/MexTK.exe" -trim "root/TM/EvLdsh.dat" || ( echo ERROR: Dat file trimming failed && exit 1 )

echo BUILD ASM FILES --------------------------------------------------------

cd "Build TM Codeset"
echo ./gecko build
./gecko build || (
    echo ERROR: gecko build failed
    cd ..
    exit 1
)
cd ..

echo BUILD START.DOL --------------------------------------------------------

mv "./root/&&systemdata/Start.dol" "Start.dol"
xdelta3 -d -f -s "Start.dol" "Build TM Start.dol/patch.xdelta" "root/&&systemdata/Start.dol" || ( echo ERROR: Failed to patch Start.dol && exit 1 )
rm "Start.dol"

echo COPYING EXTRA FILES ----------------------------------------------------

cp Additional\ ISO\ Files/codes.gct root/
cp Additional\ ISO\ Files/*.mth root/

echo REBUILD ISO --------------------------------------------------------
# mono gcr.exe --rebuild root "TM-MORE.iso" --noGameTOC
./gc_fst rebuild root TM-More.iso

echo ############ TM-More.iso has been created ######################

cleanup 0
