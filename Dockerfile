FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive \
  BUSYBOX_VERSION=1.36.1 \
  CJON_VERSION=1.7.18 \
  ELFUTILS_VERSION=0.191 \
  KERNEL_VERSION=6.9.6 \
  LIBNL_VERSION=3.9.0 \
  LIBMNL_VERSION=1.0.5 \
  LIBBPF_VERSION=1.3.0 \
  LVGL_VERSION=9.0 \
  ZLIB_VERSION=1.3.1 \
  PREFIX_DIR=/usr/glibc-compat

# Install.
RUN \
  sed -i 's/# \(.*multiverse$\)/\1/g' /etc/apt/sources.list && \
  apt-get -y update && \
  apt-get -y upgrade && \
  apt-get -qy install \
    automake \
    autoconf \
    bc \
    bison \
    build-essential \
    busybox-static \
    clang \
    cmake \
    cpio \
    dos2unix \
    flex \
    git \
    gawk \
    gettext \
    grub2 \
    grub-common \
    llvm \
    openssl \
    pkg-config \
    python3 \
    texinfo \
    rsync \
    unzip \
    wget \
    xorriso \
    xz-utils && \
  rm -rf /var/lib/apt/lists/*

RUN mkdir /build

# Kernel dependencies
# - zlib
RUN cd /build && \
  wget -qO- "https://www.zlib.net/zlib-${ZLIB_VERSION}.tar.xz" | tar Jxf - && \
  cd "zlib-${ZLIB_VERSION}" && \
  ./configure && \
  make install -j $(nproc) --silent && \
  ldconfig

# - libelf
RUN cd /build && \
  wget -qO- "https://sourceware.org/elfutils/ftp/${ELFUTILS_VERSION}/elfutils-${ELFUTILS_VERSION}.tar.bz2" | tar jxf - && \
  cd "elfutils-${ELFUTILS_VERSION}" && \
  ./configure --disable-libdebuginfod --disable-debuginfod && \
  make install -j $(nproc) --silent && \
  cp /usr/local/lib/libelf* /lib/x86_64-linux-gnu/ && \
  ldconfig

# Kernel
RUN cd /build && \
  wget -qO- "https://mirrors.edge.kernel.org/pub/linux/kernel/v${KERNEL_VERSION%%.*}.x/linux-${KERNEL_VERSION}.tar.xz" | tar Jxf -
ADD .config /build/linux-${KERNEL_VERSION}/.config
RUN cd "/build/linux-${KERNEL_VERSION}" && \
  ARCH=x86_64 make -j $(nproc) --silent && \
  ARCH=x86_64 make headers_install -j $(nproc) --silent && \
  cp "/build/linux-${KERNEL_VERSION}/arch/x86/boot/bzImage" /build/bzImage

# LVGL app dependencies
# - libbpf
RUN cd /build && \
  wget -qO- "https://github.com/libbpf/libbpf/archive/refs/tags/v${LIBBPF_VERSION}.tar.gz" | tar zxf - && \
  cd "libbpf-${LIBBPF_VERSION}/src" && \
  LIBDIR=/lib/x86_64-linux-gnu make install -j $(nproc) --silent && \
  ldconfig

# - libnl-3
RUN cd /build && \
  wget -qO- "https://github.com/thom311/libnl/releases/download/libnl$(echo ${LIBNL_VERSION}|tr '.' '_')/libnl-${LIBNL_VERSION}.tar.gz" | tar zxf - && \
  cd "libnl-${LIBNL_VERSION}" && \
  ./configure --enable-static && \
  make install -j $(nproc) --silent && \
  ldconfig

# - libmnl
RUN cd /build && \
  wget -qO- "https://www.netfilter.org/pub/libmnl/libmnl-${LIBMNL_VERSION}.tar.bz2" | tar jxf - && \
  cd "libmnl-${LIBMNL_VERSION}" && \
  ./configure --enable-static && \
  make install -j $(nproc) --silent && \
  ldconfig

# - libcjson
RUN cd /build && \
  wget -q "https://github.com/DaveGamble/cJSON/archive/refs/tags/v${CJON_VERSION}.zip" && \
  unzip "v${CJON_VERSION}.zip" && \
  cd "cJSON-${CJON_VERSION}" && \
  mkdir build && \
  cd build && \
  cmake .. -DBUILD_SHARED_AND_STATIC_LIBS=On && \
  make install -j $(nproc) --silent && \
  ldconfig

# - ethtool
RUN cd /build/ && \
  git clone --recursive https://git.launchpad.net/ubuntu/+source/ethtool && \
  cd ethtool && \
  git switch "ubuntu/noble" && \
  ./autogen.sh && \
  ./configure --disable-pretty-dump && \
  make -j $(nproc) CFLAGS="-O2 -static" --silent

# - busybox
RUN cd /build && \
  wget -qO- "https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2" | tar jxf -
ADD .config.busybox /build/busybox-${BUSYBOX_VERSION}/.config
RUN cd "/build/busybox-${BUSYBOX_VERSION}" && \
  ARCH=x86_64 make -j $(nproc) --silent

# LVGL app
RUN cd /build && \
  git clone --recursive https://github.com/lvgl/lv_port_linux_frame_buffer && \
  cd lv_port_linux_frame_buffer && \
  git switch "release/v${LVGL_VERSION}"
ADD CMakeLists.txt /build/lv_port_linux_frame_buffer/
ADD custom.cmake /build/lv_port_linux_frame_buffer/lvgl/env_support/cmake/custom.cmake
ADD app /build/lv_port_linux_frame_buffer
RUN cd /build/lv_port_linux_frame_buffer && \
  cmake . && \
  make -j $(nproc) --silent

# XDP kernel program
ADD xdp/xdp_redirect.c /build/
RUN cd /build/ && \
  clang -g -c -O2 -target bpf -I/usr/include/x86_64-linux-gnu/ -c xdp_redirect.c -o xdp_redirect.o

# Building initramfs
ADD vxspan.json /build/vxspan.json
RUN cd /build && \
  mkdir initramfs && \
  cd initramfs && \
  mkdir -p bin dev proc sys 
ADD etc /build/initramfs/etc
RUN dos2unix /build/initramfs/etc/inittab /build/initramfs/etc/init.d/rcS
RUN cd /build/initramfs && \
  cp /build/lv_port_linux_frame_buffer/bin/main \
     /build/ethtool/ethtool \
     /build/busybox-${BUSYBOX_VERSION}/busybox \
     bin/ && \
  chmod +x bin/* && \
  ln -s /bin/busybox init && \
  ln -s /bin/busybox bin/loadkmap && \
  ln -s /bin/busybox bin/mdev && \
  ln -s /bin/busybox bin/mount && \
  ln -s /bin/busybox bin/sh && \
  cp /build/xdp_redirect.o . && \
  cp /build/vxspan.json . && \
  find . -print0 | cpio --null -o --format=newc | xz --format=lzma > /build/rootfs.xz

# Building ISO
RUN cd /build && \
  mkdir -p /build/iso/boot/grub && \
  cp /build/bzImage   /build/iso/boot/bzImage && \
  cp /build/rootfs.xz /build/iso/boot/initramfs && \
  echo "set default=0\n\
set timeout=0\n\
menuentry 'vxspan' --class os {\n\
    linux /boot/bzImage vga=789 tsc=unstable ipv6.disable=1 init=/bin/sh\n\
    initrd /boot/initramfs\n\
}" > /build/iso/boot/grub/grub.cfg && \
  grub-mkrescue --install-modules="biosdisk iso9660 multiboot" --fonts="ascii" -o vxspan.iso iso/
