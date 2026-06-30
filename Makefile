.DEFAULT_GOAL := help
.PHONY: docker docker-shell build build-inside build-media test verify-mqtt deploy-runtime flash flash-dry-run status extract patch pack clean help

BUILD_DIR = cramfs
FILE = mtd4
DATE := $(shell date +%y%m%d-%H%M)
IMAGE = wibox-build-tool:latest
WIBOX_IP ?= 192.168.0.196
WIBOX_USER ?= root
WIBOX_PASS ?= qv2008

# ── one-time: build the Docker build-tool ──────────────────────────
docker:
	docker build -t $(IMAGE) .

docker-shell:
	docker run --rm -it -v $(PWD):/build $(IMAGE) bash

# ── build firmware image ───────────────────────────────────────────
build: build-media
	docker run --rm -v $(PWD):/build $(IMAGE) make build-inside

build-media:
	./build-wibox-media-daemon.sh
	rm -f src/sip_media/sip_media src/sip_media/wibox-media-daemon src/sip_media/*.o
	rm -f src/video_rtp_bridge/video_rtp_bridge

test:
	tests/mqtt_native_mock.py

verify-mqtt:
	scripts/verify_mqtt.py

deploy-runtime: build-media
	WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/deploy_runtime.sh

flash: build
	WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		CONFIRM_FLASH=$(CONFIRM_FLASH) scripts/flash_firmware.sh release/latest

flash-dry-run: build
	WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		CONFIRM_FLASH=YES FLASH_DRY_RUN=1 scripts/flash_firmware.sh release/latest

status:
	WIBOX_IP=$(WIBOX_IP) WIBOX_USER=$(WIBOX_USER) WIBOX_PASS=$(WIBOX_PASS) \
		scripts/device_status.sh

build-inside: extract patch pack

extract:
	rm -rf $(BUILD_DIR)
	cramfsck -x $(BUILD_DIR) $(FILE)

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

# ── cleanup ────────────────────────────────────────────────────────
clean:
	rm -f include/sbin/dropbearmulti include/sbin/dropbear include/sbin/dropbearkey include/sbin/dropbearconvert
	rm -f include/bin/scp include/bin/dbclient
	rm -f src/sip_media/sip_media src/sip_media/wibox-media-daemon src/sip_media/*.o
	rm -f src/video_rtp_bridge/video_rtp_bridge
	rm -rf $(BUILD_DIR) patch.log 2>/dev/null

# ── help ───────────────────────────────────────────────────────────
help:
	@echo "WiBox Firmware Builder"
	@echo ""
	@echo "  1. make docker    Build Docker build-tool (one-time)"
	@echo "  2. make build     Build media binaries and firmware image (cramfs)"
	@echo "  3. make build-media  Build wibox-media-daemon"
	@echo "  4. make test      Run host-side regression tests"
	@echo "  5. make verify-mqtt  Verify Home Assistant MQTT discovery/state"
	@echo "  6. make deploy-runtime  Upload current daemon to /tmp and restart it"
	@echo "  7. make flash CONFIRM_FLASH=YES  Flash release/latest to mtd4"
	@echo "  8. make flash-dry-run  Validate flash upload/checks without writing mtd4"
	@echo "  9. make status    Show WiBox runtime status"
	@echo ""
	@echo "  Prerequisite: factory mtd4 backup at ./mtd4"
	@echo "  Output:        release/image-YYMMDD-HHMM"
	@echo ""
	@echo "  make clean      Remove build artifacts"
	@echo "  make docker-shell  Open shell inside build-tool"
