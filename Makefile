.PHONY: clean iso all release

dats = root/codes.gct root/TM/EvLdsh.dat root/TM/EvWdsh.dat root/TM/EvLCl.dat root/TM/EvLabCSS.dat root/TM/EvLab.dat root/TM/TmDt.dat

clean:
	rm -rf TM-More.iso
	rm -rf ./root
	rm -rf ./build/
	rm -rf TM-More/

root/:
	rm -rf ./build
	rm -rf ./root
	./gc_fst extract ${iso}
	mkdir root/TM/
	./gc_fst set-header "root/&&systemdata/ISO.hdr" GTME01 "Melee Training Mode - More"
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

root/codes.gct: root/ Additional\ ISO\ Files/*
	cd "Build TM Codeset" && ./gecko build
	cp Additional\ ISO\ Files/codes.gct root/
	cp -f Additional\ ISO\ Files/opening.bnr root/
	cp Additional\ ISO\ Files/*.mth root/

root/&&systemdata/Start.dol: root/
	mv "./root/&&systemdata/Start.dol" "Start.dol"
	xdelta3 -d -f -s "Start.dol" "Build TM Start.dol/patch.xdelta" "root/&&systemdata/Start.dol"
	rm "Start.dol"

TM-More.iso: root/&&systemdata/Start.dol $(dats)
	./gc_fst rebuild root TM-More.iso

iso: TM-More.iso

TM-More.zip: TM-More.iso Release\ Scripts/*
	rm -rf TM-More/
	mkdir TM-More
	cp -rf --parents "root/&&systemdata/Start.dol" "root/&&systemdata/ISO.hdr" $(dats) TM-More/
	mv TM-More/root/ TM-More/patch/
	cp Additional\ ISO\ Files/codes.gct TM-More/patch/
	cp -f Additional\ ISO\ Files/opening.bnr TM-More/patch/
	cp Additional\ ISO\ Files/*.mth TM-More/patch/
	cp -r Release\ Scripts/* TM-More/
	cp gc_fst TM-More/
	cp gc_fst.exe TM-More/
	zip -r TM-More.zip TM-More/
	rm -r TM-More/

release: TM-More.zip

all: iso release

