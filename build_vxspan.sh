#!/bin/bash

for cmd in docker; do
	if ! command -v $cmd &> /dev/null
	then
		echo "This script requires the '$cmd' command"
		exit 1
	fi
done
if [ $EUID -ne 0 ] && [ $(groups | grep -c docker) -ne 1 ]; then
	echo "You must have docker privileges to use this script"
	exit 1
fi

mkdir build release

# Application compilation
echo "Building VxSpan:"
docker build . -t vxspan-build \
	&& echo "Done!" \
	|| exit 1

# ISO
echo "Extracting ISO to 'release/' directory:"
docker run --rm vxspan-build cat /build/vxspan.iso > release/vxspan.iso \
	&& echo "- vxspan.iso"

# Artefacts
echo "Extracting build artefacts to 'build/' directory:"
docker run --rm vxspan-build cat /build/lv_port_linux_frame_buffer/bin/main > build/main           \
	&& echo "- main"
docker run --rm vxspan-build cat /build/xdp_redirect.o                      > build/xdp_redirect.o \
	&& echo "- xdp_redirect.o"
docker run --rm vxspan-build cat /build/bzImage                             > build/bzImage        \
	&& echo "- bzImage"
docker run --rm --workdir=/build/initramfs vxspan-build sh -c 'find . -print0 | cpio --null -o --format=newc | xz --format=lzma' > build/rootfs.xz \
	&& echo "- rootfs.xz"
