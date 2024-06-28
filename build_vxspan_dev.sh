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

mkdir build-dev release-dev

# Application compilation
echo "Building VxSpan:"
docker build . -f Dockerfile.dev -t vxspan-dev \
	&& echo "Done!" \
	|| exit 1

# ISO
echo "Extracting ISO to 'release-dev/' directory:"
docker run --rm vxspan-dev cat /build/vxspan.iso > release-dev/vxspan.iso \
	&& echo "- vxspan.iso"

# Artefacts
echo "Extracting build artefacts to 'build/' directory:"
docker run --rm vxspan-dev cat /build/lv_port_linux_frame_buffer/bin/main > build-dev/main           \
	&& echo "- main"
docker run --rm vxspan-dev cat /build/xdp_redirect.o                      > build-dev/xdp_redirect.o \
	&& echo "- xdp_redirect.o"
docker run --rm vxspan-dev cat /build/bzImage                             > build-dev/bzImage        \
	&& echo "- bzImage"
docker run --rm --workdir=/build/initramfs vxspan-dev sh -c 'find . -print0 | cpio --null -o --format=newc | xz --format=lzma' > build-dev/rootfs.xz \
	&& echo "- rootfs.xz"
