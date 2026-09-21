.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET := llama-3ds
BUILD := build
SOURCES := source
INCLUDES := $(TOPDIR)/include $(TOPDIR)/.deps/llama.cpp/include $(TOPDIR)/.deps/llama.cpp/ggml/include

APP_TITLE := llama-3ds
APP_DESCRIPTION := Local llama.cpp chat for New Nintendo 3DS/2DS XL
APP_AUTHOR := llama-3ds contributors

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -Os -mword-relocations -ffunction-sections $(ARCH)
CFLAGS += $(INCLUDE) -D__3DS__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -Wl,--gc-sections
LIBS := -Wl,--start-group -lllama -lggml -Wl,--whole-archive -lggml-cpu -Wl,--no-whole-archive -lggml-base -Wl,--end-group -lctru -lm
LIBDIRS := $(CTRULIB) $(TOPDIR)/.deps/llama-build-3ds/src $(TOPDIR)/.deps/llama-build-3ds/ggml/src $(TOPDIR)/.deps/llama-build-3ds/ggml/src/ggml-cpu

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib -L$(dir))
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: all clean runtime
all: runtime $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
runtime:
	@bash tools/build_llama_cpp_3ds.sh
$(BUILD):
	@mkdir -p $@
clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).map .deps/llama-build-3ds
else
$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
