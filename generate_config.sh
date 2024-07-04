#!/bin/bash
#/bin/bash
if [ `id -u` -ne 0 ]
  then echo "Please run this script as root or using sudo!"
  exit
fi

for cmd in file grep cpio xz grub-mkrescue xorriso; do
	if ! command -v $cmd &> /dev/null
	then
		echo "This script requires the '$cmd' command"
		exit 1
	fi
done
if [[ -d "$1" ]]; then
	if [[ -f "$2" ]] \
		&& file $2 | grep "ISO 9660 CD-ROM filesystem data (DOS/MBR boot sector) 'ISOIMAGE' (bootable)"; then

		tmpiso=$(mktemp -d)
		tmpifs=$(mktemp -d)
		currdir=$(pwd)
		echo -e "\e[01;01m;Extracting \e[01;32mvxspan.iso\e[0m"
		xorriso -osirrox on -indev $currdir/$2 -extract / $tmpiso
		cd $tmpifs
		xzcat --format=lzma $tmpiso/boot/initramfs \
			| cpio -idm
		for path in $(/bin/ls $currdir/$1/*.json); do
			file=${path##*/}
			echo -e "\e[01;01mGenerating \e[01;32mvxspan_${file%.*}.iso\e[01;01m for \e[01;32m${file}\e[0m"
			cp $path vxspan.json;
			find . -print0 \
				| cpio --null -o --format=newc \
				| xz --format=lzma > $tmpiso/boot/initramfs && \
			grub-mkrescue \
				--install-modules="biosdisk iso9660 multiboot" \
				--fonts="ascii" \
				-o $currdir/$1/vxspan_${file%.*}.iso $tmpiso
		done
		cd $currdir
		rm -fr $tmpiso
		rm -fr $tmpifs
		echo -e "\e[01;01mDone:\e[0m"
		find $1/ -type f | sort
	else
		echo "No source vxspan.iso specified"
		echo "Usage ${0##*/} <config_dir> <vxspan.iso>"
	fi
else
	echo "No source directory specified"
	echo "Usage ${0##*/} <config_dir> <vxspan.iso>"
fi