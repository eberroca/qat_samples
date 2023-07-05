#!/usr/bin/bash

ICP_ROOT="/QAT"
SOURCE_FILES=(sleep mem comp poll cback main)

for i in "${SOURCE_FILES[@]}"
do
    rm "$i.o"
    gcc -I$ICP_ROOT/quickassist/include -I$ICP_ROOT/quickassist/include/dc -I$ICP_ROOT/quickassist/utilities/libusdm_drv -I$ICP_ROOT/quickassist/utilities/libusdm_drv/include -I$ICP_ROOT/quickassist/lookaside/access_layer/include -c $i.c -Wunused
done
rm main
`echo -e "gcc \c"; for i in "${SOURCE_FILES[@]}"; do echo -e "$i.o \c"; done; echo "-o main -lqat_s -lusdm_drv_s -lpthread"`
