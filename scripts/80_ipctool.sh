#!/bin/sh
set -eu

FILE=include/bin/ipctool
IPCTOOL_TAG=latest
IPCTOOL_TAG_COMMIT=2082b3270d4b878395ab626d1fcbce25a8087f1b
IPCTOOL_SHA256=2f3cc67dc39df2996eed4c911092f91919eb82f6a4d82c2edf3d8fd0979f1112
FILE_DOWNLOAD=https://github.com/OpenIPC/ipctool/releases/download/${IPCTOOL_TAG}/ipctool

if [ ! -d "include/" ]; then
  echo "[*] Folder include does not exist, skipping."
  exit
fi

verify_ipctool() {
  actual=$(sha256sum "${FILE}" | cut -d ' ' -f 1)
  if [ "${actual}" != "${IPCTOOL_SHA256}" ]; then
    echo "[!] ipctool checksum mismatch"
    echo "    expected: ${IPCTOOL_SHA256}"
    echo "    actual:   ${actual}"
    return 1
  fi
}

verify_tag() {
  if ! command -v git >/dev/null 2>&1; then
    echo "[*] git not available; relying on ipctool checksum pin"
    return
  fi
  actual=$(git ls-remote --tags https://github.com/OpenIPC/ipctool.git "refs/tags/${IPCTOOL_TAG}" | awk '{print $1}')
  if [ "${actual}" != "${IPCTOOL_TAG_COMMIT}" ]; then
    echo "[!] OpenIPC/ipctool tag moved"
    echo "    expected: ${IPCTOOL_TAG_COMMIT}"
    echo "    actual:   ${actual}"
    exit 1
  fi
}

if [ -e "${FILE}" ]; then
  if verify_ipctool; then
    echo "[*] ipctool already downloaded and checksum verified."
    exit
  fi
  echo "[*] Re-downloading pinned ipctool"
  rm -f "${FILE}"
fi

verify_tag

echo "[*] Downloading ipctool"
if command -v curl >/dev/null ; then
  curl -fL -o ${FILE} ${FILE_DOWNLOAD}
else
  wget -q -O ${FILE} ${FILE_DOWNLOAD}
fi

verify_ipctool
chmod 755 ${FILE}
