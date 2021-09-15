#!/bin/bash

export PATH=/opt/marvell/cross/bin:$PATH

G_PARAMS=""

# do_compile <le / be> <board> <NAND/NOR/SPI> <NANDBOOT=1 ...>
do_compile()
{
	ENDIANESS=$1
	BOARD=$2
	PARAMS=$G_PARAMS

	ENDIAN=""
	COMPILER="arm-mv5sft-linux-gnueabi-"

	if [ $ENDIANESS == "be" ]; then
		ENDIAN="BE=1"
		COMPILER="armeb-mv5sft-linux-gnueabi-"
	fi

	case $BOARD in
		f660 )
			TEMP="F660"
			;;
        	* )
			TEMP="ERROR"
	esac

	export CROSS_COMPILE=$COMPILER
	make mrproper
	make $BOARD"_config" $PARAMS $ENDIAN
	make -s >> log.txt
}

export ARCH=arm

G_PARAMS="NANDBOOT=1 NAND=1 USB=1"

do_compile le f660

