#!/bin/bash

if [ $# -ne 1 ] && [ $# -ne 2 ]
then
	echo "usage: bash build.sh config_file [output_file]"
	exit 1
fi

if [ ! -f $1  ]
then
	echo "Error: $1 file not found."
	exit 1
fi

g++ config_reader.cpp -o config_reader
./config_reader $1 > configHeader.h
ERROR_CODE=$?
if [ $ERROR_CODE -ne 0 ]; then
    echo "error"
    exit 1
fi

make clean

if [ $# -eq 2 ]; then
	make output_file=$2
else
	make output_file="teste"
fi

rm config_reader
rm configHeader.h
make clean
