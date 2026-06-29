################################################################################
################################################################################
##
## Makefile -- makefile for audio_bridge
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
    elif [ -n "$container" ]; then \
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
else
    # Running on host - use original toolchain
    TOPDIR = ../../
    CC = $(TOPDIR)/tools/toolchain/arm-linux/4.6.1/usr/bin/arm-goke-linux-uclibcgnueabi-gcc
endif

################################################################################
# source files
################################################################################

SOURCES += audio_bridge.c
SOURCES += ap.c
SOURCES += config.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

################################################################################
# define user cc flags
################################################################################

ifeq ($(IS_CONTAINER),true)
    # Container paths
    CCFLAGS_USER += -I/opt/gk710x/adi/include
    CCFLAGS_USER += -I/opt/gk710x/ap
else
    # Host paths
    CCFLAGS_USER += -I$(TOPDIR)/adi/include
    CCFLAGS_USER += -I$(TOPDIR)/subsystem/audio_processing/ap
endif

CCFLAGS_USER += -O3 -DDEBUG -g

################################################################################
# define library paths and libraries
################################################################################

ifeq ($(IS_CONTAINER),true)
    # Container library paths
    LDFLAGS_USER += -L/usr/lib
else
    # Host library paths
    LDFLAGS_USER += -L$(TOPDIR)/adi/lib/uClibc
    LDFLAGS_USER += -L$(TOPDIR)/subsystem/audio_processing/ap/lib/uClibc
endif

# ADD RPATH to look for libraries next to executable and the /usr/lib
LDFLAGS_USER += -Wl,-rpath=.:/usr/lib
LIBRARIES += adi pthread ap m

################################################################################
# define target name
################################################################################

PROGRAM = audio_bridge

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
# Build rule
################################################################################

# Make 'all' the default target
.DEFAULT_GOAL := all

# Default target
all: copy_libs build_objects

# For backwards compatibility, add 'default' target
default: copy_libs build_objects

copy_libs:
	@echo "Copying required libraries..."
	@cp $(TOPDIR)/adi/lib/uClibc/libadi.so . 2>/dev/null || echo "Warning: libadi.so not found"
	@cp $(TOPDIR)/subsystem/audio_processing/ap/lib/uClibc/libap.so . 2>/dev/null || echo "Warning: libap.so not found"

# Object-based build with proper dependencies
build_objects: $(OBJECTS)
	@echo "Linking $(PROGRAM) from object files..."
	$(CC) $(LDFLAGS_USER) $(OBJECTS) $(addprefix -l,$(LIBRARIES)) -o $(PROGRAM)
	@echo "Build complete: $(PROGRAM)"

# Generic object compilation rule
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CCFLAGS_USER) -c $< -o $@

# Header dependencies
audio_bridge.o: audio_bridge.c config.h ap.h
config.o: config.c config.h
ap.o: ap.c ap.h

deploy: default
	@echo "Deploying to $(DEVICE_TARGET)..."
	@$(SCP_CMD) $(PROGRAM) libadi.so libap.so $(DEVICE_TARGET)/
	@echo "Deployment complete!"
	@echo "Run on device: cd $(DEVICE_PATH) && ./$(PROGRAM)"

clean:
	@echo "Cleaning..."
	@rm -f $(PROGRAM) *.o
	@echo "Clean complete"

# Show build info
info:
	@echo "=== Build Configuration ==="
	@echo "Container Environment: $(CONTAINER_ENV)"
	@echo "Is Container: $(IS_CONTAINER)"
	@echo "CC: $(CC)"
	@echo "TOPDIR: $(TOPDIR)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "PROGRAM: $(PROGRAM)"
	@echo "LIBRARIES: $(LIBRARIES)"
	@echo "DEVICE_TARGET: $(DEVICE_TARGET)"
	@echo "=========================="

.PHONY: all default copy_libs build_objects deploy clean info
