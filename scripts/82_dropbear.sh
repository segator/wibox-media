#!/bin/sh
set -e

PACKAGE_NAME=dropbear
PACKAGE_VERSION=2020.81
PACKAGE_DOWNLOAD=https://matt.ucc.asn.au/dropbear/releases/dropbear-${PACKAGE_VERSION}.tar.bz2
FILE=include/sbin/dropbear

if [ ! -d "include/" ]; then
  echo "[*] Folder include does not exist, skipping."
  exit
fi
if [ -e "${FILE}" ]; then
  echo "[*] ${PACKAGE_NAME} already built, skipping."
  exit
fi

echo "[*] Downloading ${PACKAGE_NAME} ${PACKAGE_VERSION}"
wget -q -O ${PACKAGE_DOWNLOAD##*/} ${PACKAGE_DOWNLOAD}

mkdir -p ${PACKAGE_NAME}
tar xf ${PACKAGE_DOWNLOAD##*/} -C ${PACKAGE_NAME} --strip-components=1

echo "[*] Patching"
sed -i 's!/etc/dropbear!/mnt/mtd/dropbear!g' ${PACKAGE_NAME}/default_options.h
sed -i 's!DO_MOTD 1!DO_MOTD 0!' ${PACKAGE_NAME}/default_options.h

echo "[*] Building ${PACKAGE_NAME}"
cd ${PACKAGE_NAME}
./configure --enable-static --disable-zlib --disable-lastlog \
  --host=arm-goke-linux-uclibcgnueabi --target=arm-goke-linux-uclibcgnueabi
make PROGRAMS="dropbear dropbearkey" MULTI=1
arm-goke-linux-uclibcgnueabi-strip dropbearmulti
cd ..

echo "[*] Installing"
mkdir -p include/sbin
cp ${PACKAGE_NAME}/dropbearmulti include/sbin/
for n in dropbear dropbearkey; do
  ln -sf dropbearmulti include/sbin/${n}
done
rm -rf ${PACKAGE_NAME} ${PACKAGE_DOWNLOAD##*/}
echo "[*] ${PACKAGE_NAME} built successfully"
