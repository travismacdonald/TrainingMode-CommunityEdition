#!/bin/bash

# Hi linux users!

set -e

if [ -z ${1} ]; then
    echo "Usage: build.sh <path/to/melee.iso>"
    exit 1
fi

echo Building TM-More.iso...
echo If the provided gc_fst binary fails, you can build your own from https://github.com/AlexanderHarrison/gc_fst

./gc_fst extract ${1}
rm -rf root/MvHowto.mth root/MvOmake15.mth root/MvOpen.mth
cd patch && cp --parents -r * ../root/ && cd ..
./gc_fst rebuild root "TM-More.iso" 
rm -rf ./root

echo "############ TM-More.iso has been created ######################"
