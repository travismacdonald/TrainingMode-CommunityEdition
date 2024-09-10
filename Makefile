.PHONY: clean all

clean:
	rm -rf TM-More.iso
	rm -rf ./root
	rm -rf ./build/

root/:
	rm -rf ./root
	./gc_fst extract ${iso}
	mkdir root/TM/
	rm root/MvHowto.mth root/MvOmake15.mth root/MvOpen.mth

root/TM/TmDt.dat: root/ patch/tmdata/*
	cp "patch/tmdata/assets/evMenu.dat" "root/TM/TmDt.dat"
	mono "MexTK/MexTK.exe" -ff -i "patch/tmdata/source/events.c" -b "build" -s tmFunction -dat "root/TM/TmDt.dat" -t "MexTK/tmFunction.txt" -q -ow -l "MexTK/melee.link" -op 1
	mono "MexTK/MexTK.exe" -trim "root/TM/TmDt.dat"

root/TM/EvLab.dat: root/ patch/events/lab/*
	cp "patch/events/lab/assets/labData.dat" "root/TM/EvLab.dat"
	mono "MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab.c" -b "build" -s evFunction -dat "root/TM/EvLab.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1
	mono "MexTK/MexTK.exe" -trim "root/TM/EvLab.dat"

root/TM/EvLabCSS.dat: root/ patch/events/lab/*
	cp "patch/events/lab/assets/importData.dat" "root/TM/EvLabCSS.dat"
	mono "MexTK/MexTK.exe" -ff -i "patch/events/lab/source/lab_css.c" -b "build" -s cssFunction -dat "root/TM/EvLabCSS.dat" -t "MexTK/cssFunction.txt" -q -ow -l "MexTK/melee.link" -op 1
	mono "MexTK/MexTK.exe" -trim "root/TM/EvLabCSS.dat"

root/TM/EvLCl.dat: root/ patch/events/lcancel/*
	cp "patch/events/lcancel/assets/lclData.dat" "root/TM/EvLCl.dat"
	mono "MexTK/MexTK.exe" -ff -i "patch/events/lcancel/source/lcancel.c" -b "build" -s evFunction -dat "root/TM/EvLCl.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1
	mono "MexTK/MexTK.exe" -trim "root/TM/EvLCl.dat"

root/TM/EvLdsh.dat: root/ patch/events/ledgedash/*
	cp "patch/events/ledgedash/assets/ldshData.dat" "root/TM/EvLdsh.dat"
	mono "MexTK/MexTK.exe" -ff -i "patch/events/ledgedash/source/ledgedash.c" -b "build" -s evFunction -dat "root/TM/EvLdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1
	mono "MexTK/MexTK.exe" -trim "root/TM/EvLdsh.dat"

root/TM/EvWdsh.dat: root/ patch/events/wavedash/*
	cp "patch/events/wavedash/assets/wdshData.dat" "root/TM/EvWdsh.dat"
	mono "MexTK/MexTK.exe" -ff -i "patch/events/wavedash/source/wavedash.c" -b "build" -s evFunction -dat "root/TM/EvWdsh.dat" -t "MexTK/evFunction.txt" -q -ow -l "MexTK/melee.link" -op 1
	mono "MexTK/MexTK.exe" -trim "root/TM/EvLdsh.dat"

root/codes.gct: root/
	cd "Build TM Codeset" && ./gecko build
	cp Additional\ ISO\ Files/codes.gct root/
	cp -f Additional\ ISO\ Files/opening.bnr root/
	cp Additional\ ISO\ Files/*.mth root/

root/&&systemdata/Start.dol: root/
	mv "./root/&&systemdata/Start.dol" "Start.dol"
	xdelta3 -d -f -s "Start.dol" "Build TM Start.dol/patch.xdelta" "root/&&systemdata/Start.dol"
	rm "Start.dol"

TM-More.iso: root/&&systemdata/Start.dol root/codes.gct root/TM/EvLdsh.dat root/TM/EvWdsh.dat root/TM/EvLCl.dat root/TM/EvLabCSS.dat root/TM/EvLab.dat root/TM/TmDt.dat
	./gc_fst rebuild root TM-More.iso

all: TM-More.iso
