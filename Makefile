.PHONY: clean iso all release

dats = root/TM/ledgedash.dat root/TM/wavedash.dat root/TM/lcancel.dat root/TM/labCSS.dat root/TM/eventMenu.dat root/TM/lab.dat

# find all .asm and .s files in the ASM dir. We have the escape the spaces, so we pipe to sed
ASM_FILES := $(shell find ASM -type f \( -name '*.asm' -o -name '*.s' \) | sed 's/ /\\ /g')

MEX_BUILD=mono MexTK/MexTK.exe -ff -b "build" -q -ow -l "MexTK/melee.link" -op 2
MEX_TRIM=mono MexTK/MexTK.exe -trim

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

root/TM/eventMenu.dat: root/ src/events.c src/events.h
	cp "dats/eventMenu.dat" "root/TM/eventMenu.dat" 
	$(MEX_BUILD) -i "src/events.c" -s "tmFunction" -dat "root/TM/eventMenu.dat" -t "MexTK/tmFunction.txt"
	$(MEX_TRIM) "root/TM/eventMenu.dat"

root/TM/lab.dat: root/ src/lab.c src/lab.h src/lab_common.h
	cp "dats/lab.dat" "root/TM/lab.dat"
	$(MEX_BUILD) -i "src/lab.c" -s "evFunction" -dat "root/TM/lab.dat" -t "MexTK/evFunction.txt"
	$(MEX_TRIM) "root/TM/lab.dat"

root/TM/labCSS.dat: root/ src/lab_css.c src/lab_common.h
	cp "dats/labCSS.dat" "root/TM/labCSS.dat"
	$(MEX_BUILD) -i "src/lab_css.c" -s "cssFunction" -dat "root/TM/labCSS.dat" -t "MexTK/cssFunction.txt"
	$(MEX_TRIM) "root/TM/labCSS.dat"

root/TM/lcancel.dat: root/ src/lcancel.c src/lcancel.h
	cp "dats/lcancel.dat" "root/TM/lcancel.dat"
	$(MEX_BUILD) -i "src/lcancel.c" -s "evFunction" -dat "root/TM/lcancel.dat" -t "MexTK/evFunction.txt"
	$(MEX_TRIM) "root/TM/lcancel.dat"

root/TM/ledgedash.dat: root/ src/ledgedash.c src/ledgedash.h
	cp "dats/ledgedash.dat" "root/TM/ledgedash.dat"
	$(MEX_BUILD) -i "src/ledgedash.c" -s "evFunction" -dat "root/TM/ledgedash.dat" -t "MexTK/evFunction.txt"
	$(MEX_TRIM) "root/TM/ledgedash.dat"

root/TM/wavedash.dat: root/ src/wavedash.c src/wavedash.h
	cp "dats/wavedash.dat" "root/TM/wavedash.dat"
	$(MEX_BUILD) -i "src/wavedash.c" -s "evFunction" -dat "root/TM/wavedash.dat" -t "MexTK/evFunction.txt"
	$(MEX_TRIM) "root/TM/wavedash.dat"

root/codes.gct: root/ Build\ TM\ Codeset/codes.json Additional\ ISO\ Files/opening.bnr $(ASM_FILES)
	cd "Build TM Codeset" && ./gecko build
	cp Additional\ ISO\ Files/* root/

root/&&systemdata/Start.dol: root/
	mv "./root/&&systemdata/Start.dol" "Start.dol"
	xdelta3 -d -f -s "Start.dol" "Build TM Start.dol/patch.xdelta" "root/&&systemdata/Start.dol"
	rm "Start.dol"

TM-More.iso: root/&&systemdata/Start.dol root/codes.gct $(dats)
	./gc_fst rebuild root TM-More.iso

iso: TM-More.iso

TM-More.zip: TM-More.iso
	zip TM-More.zip TM-More.iso

release: TM-More.zip

all: iso release

