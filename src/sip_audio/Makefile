################################################################################
################################################################################
##
## Makefile -- makefile for sip_audio
##
## (C) Copyright 2025 by TODO
##
################################################################################
################################################################################

# Detect if running in a container (Docker or Podman)
CONTAINER_ENV := $(shell \
    if [ -f /.dockerenv ]; then \
        echo "docker"; \
    elif [ -f /run/.containerenv ]; then \
        echo "podman"; \
    elif [ -n "$$container" ]; then \
        echo "container"; \
    else \
        echo "host"; \
    fi \
)

# Check if running in any container
IS_CONTAINER := $(shell \
    if [ "$(CONTAINER_ENV)" != "host" ]; then \
        echo "true"; \
    else \
        echo "false"; \
    fi \
)

ifeq ($(IS_CONTAINER),true)
    # Running in container - use vendor toolchain
    CC = arm-goke-linux-uclibcgnueabi-gcc
    TOPDIR = /workspace
    PJSIP_DIR = /opt/pjproject-2.11.1
else
    # Running on host - use original toolchain
    TOPDIR = ../../
    CC = $(TOPDIR)/tools/toolchain/arm-linux/4.6.1/usr/bin/arm-goke-linux-uclibcgnueabi-gcc
    PJSIP_DIR = $(TOPDIR)/applications/pjproject-2.11.1
endif

################################################################################
# source files
################################################################################

SOURCES = sip_audio.c sip_calling.c intercom.c config.c

################################################################################
# define user cc flags
################################################################################

CCFLAGS_USER = -O2 -DDEBUG -g
CCFLAGS_USER += -I$(PJSIP_DIR)/pjsip/include
CCFLAGS_USER += -I$(PJSIP_DIR)/pjlib/include
CCFLAGS_USER += -I$(PJSIP_DIR)/pjlib-util/include
CCFLAGS_USER += -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0

################################################################################
# define library paths and libraries
################################################################################

LDFLAGS_USER = -L$(PJSIP_DIR)/pjsip/lib
LDFLAGS_USER += -L$(PJSIP_DIR)/pjlib/lib
LDFLAGS_USER += -L$(PJSIP_DIR)/pjlib-util/lib
# ADD RPATH to look for libraries next to executable
LDFLAGS_USER += -Wl,-rpath,.

# PJSIP libraries
ifeq ($(IS_CONTAINER),true)
    LIBRARIES = pjsip-arm-goke-linux-uclibcgnueabi pjlib-util-arm-goke-linux-uclibcgnueabi pj-arm-goke-linux-uclibcgnueabi pthread m
else
    LIBRARIES = pjsip pjlib-util pj pthread m
endif

################################################################################
# define target name
################################################################################

PROGRAM = sip_audio

################################################################################
# Deployment configuration
################################################################################

# Default device settings (can be overridden)
DEVICE_IP ?= 172.16.0.24
DEVICE_USER ?= root
DEVICE_PATH ?= /tmp
DEVICE_PASSWORD ?= qv2008
DEVICE_TARGET = $(DEVICE_USER)@$(DEVICE_IP):$(DEVICE_PATH)

# Use sshpass if password is provided
ifdef DEVICE_PASSWORD
    SCP_CMD = sshpass -p "$(DEVICE_PASSWORD)" scp
else
    SCP_CMD = scp
endif

################################################################################
# Build dependencies
################################################################################

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Header dependencies
sip_audio.o: sip_audio.c sip_calling.h intercom.h config.h
sip_calling.o: sip_calling.c sip_calling.h
intercom.o: intercom.c intercom.h
config.o: config.c config.h

################################################################################
# Build rules
################################################################################

# Make 'all' the default target
.DEFAULT_GOAL := all

# Default target
all: $(PROGRAM)

# For backwards compatibility, add 'default' target
default: $(PROGRAM)

# Main program target
$(PROGRAM): $(OBJECTS)
	@echo "Linking $(PROGRAM) from object files: $(OBJECTS)"
	$(CC) $(LDFLAGS_USER) $(OBJECTS) $(addprefix -l,$(LIBRARIES)) -o $(PROGRAM)
	@echo "Build complete: $(PROGRAM)"

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CCFLAGS_USER) -c $< -o $@

deploy: default
	@echo "Deploying to $(DEVICE_TARGET)..."
	@$(SCP_CMD) $(PROGRAM) $(DEVICE_TARGET)/
	@$(SCP_CMD) *.so* $(DEVICE_TARGET)/ 2>/dev/null || echo "No shared libraries to deploy"
	@echo "Deployment complete!"
	@echo "Run on device: cd $(DEVICE_PATH) && ./$(PROGRAM)"

clean:
	@echo "Cleaning..."
	@rm -f $(PROGRAM) $(OBJECTS) *.so *.so.*
	@echo "Clean complete"

# Show build info
info:
	@echo "=== Build Configuration ==="
	@echo "Container Environment: $(CONTAINER_ENV)"
	@echo "Is Container: $(IS_CONTAINER)"
	@echo "CC: $(CC)"
	@echo "TOPDIR: $(TOPDIR)"
	@echo "PJSIP_DIR: $(PJSIP_DIR)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "PROGRAM: $(PROGRAM)"
	@echo "DEVICE_TARGET: $(DEVICE_TARGET)"
	@echo "=========================="

.PHONY: all info clean deploy
