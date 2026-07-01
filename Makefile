.DEFAULT_GOAL := help

.PHONY: \
	docker docker-shell build build-media prepare-base test verify verify-image \
	deploy-runtime verify-device verify-runtime verify-mqtt device-status \
	build-inside extract patch pack clean help

BUILD_DIR := cramfs
BASE_IMAGE := mtd4
DATE := $(shell date +%y%m%d-%H%M)
BUILD_IMAGE := wibox-build-tool:latest

WIBOX_IP ?= 192.168.0.196
WIBOX_USER ?= root
WIBOX_PASS ?= qv2008

docker:
	docker build -t $(BUILD_IMAGE) .

docker-shell:
	docker run --rm -it -v $(PWD):/build $(BUILD_IMAGE) bash

build: prepare-base build-media
	docker run --rm -v $(PWD):/build $(BUILD_IMAGE) make build-inside

prepare-base:
	@if [ ! -f "$(BUILD_DIR)/lib/libssl.so.1.1" ] || [ ! -f "$(BUILD_DIR)/lib/libcrypto.so.1.1" ]; then \
		echo "[*] Extracting base image for build libraries"; \
		docker run --rm -v $(PWD):/build $(BUILD_IMAGE) make extract; \
	fi

build-media: prepare-base
	BUILD_IMAGE=$(BUILD_IMAGE) scripts/build_wibox_media_daemon.sh
	rm -f src/sip_media/sip_media src/sip_media/wibox-media-daemon src/sip_media/*.o

test:
	tests/mqtt_native_mock.py

verify: test verify-image

verify-image:
	@scripts/verify_image.sh

deploy-runtime: build-media
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/deploy_runtime.sh

verify-device:
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/verify_device.sh

verify-runtime:
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/verify_runtime.sh

verify-mqtt:
	scripts/verify_mqtt.py

device-status:
	@WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/device_status.sh

build-inside: extract patch pack

extract:
	rm -rf $(BUILD_DIR)
	cramfsck -x $(BUILD_DIR) $(BASE_IMAGE)

patch:
	@for PATCH in scripts/??_*.sh; do \
		echo ">> $$PATCH"; \
		ROOTFS=$(BUILD_DIR) sh $$PATCH; \
		echo "----"; \
	done | tee -a patch.log
	@touch $(BUILD_DIR)/patched

pack:
	rm -f $(BUILD_DIR)/patched 2>/dev/null
	mkdir -p release
	mkcramfs -e 0 -v $(BUILD_DIR) release/image-$(DATE)
	ln -sf image-$(DATE) release/latest
	@echo ""
	@echo "=== BUILD DONE ==="
	@ls -la release/latest
	@md5sum release/image-$(DATE)

clean:
	rm -f include/bin/dbclient include/bin/ipctool include/bin/scp
	rm -f include/etc/wibox-release
	rm -f src/sip_media/sip_media src/sip_media/wibox-media-daemon src/sip_media/*.o
	@if docker image inspect $(BUILD_IMAGE) >/dev/null 2>&1; then \
		docker run --rm -v $(PWD):/build $(BUILD_IMAGE) bash -lc "rm -rf /build/$(BUILD_DIR) /build/.verify-image-root /build/patch.log /build/release /build/include/sbin"; \
	else \
		rm -rf $(BUILD_DIR) .verify-image-root patch.log release include/sbin 2>/dev/null || true; \
	fi

help:
	@echo "WiBox Media"
	@echo ""
	@echo "Common:"
	@echo "  make docker          Build the local firmware build image"
	@echo "  make build           Build media binaries and release/latest"
	@echo "  make build-media     Build wibox-media-daemon and firmware_update only"
	@echo "  make test            Run host MQTT regression tests"
	@echo "  make verify          Run local tests and verify release/latest"
	@echo "  make verify-image    Inspect release/latest contents"
	@echo "  make clean           Remove local build artifacts"
	@echo ""
	@echo "Device development:"
	@echo "  make deploy-runtime  Run current daemon from /tmp on a WiBox"
	@echo "  make verify-device   Verify active runtime and MQTT against a WiBox"
	@echo "  make device-status   Show process, config and recent daemon log"
	@echo ""
	@echo "Variables:"
	@echo "  WIBOX_IP=$(WIBOX_IP)"
	@echo "  WIBOX_USER=$(WIBOX_USER)"
	@echo "  WIBOX_PASS=<hidden>"
	@echo ""
	@echo "First install and recovery are documented in docs/getting_started.md and docs/recovery.md."
	@echo "Routine upgrades use /usr/bin/firmware_update or Home Assistant."
