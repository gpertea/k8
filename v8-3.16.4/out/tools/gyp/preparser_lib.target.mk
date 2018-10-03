# This file is generated by gyp; do not edit.

TOOLSET := target
TARGET := preparser_lib
DEFS_Debug := \
	'-DENABLE_DEBUGGER_SUPPORT' \
	'-DV8_TARGET_ARCH_X64' \
	'-DDEBUG' \
	'-DENABLE_DISASSEMBLER' \
	'-DV8_ENABLE_CHECKS' \
	'-DOBJECT_PRINT' \
	'-DVERIFY_HEAP' \
	'-DENABLE_EXTRA_CHECKS'

# Flags passed to all source files.
CFLAGS_Debug := \
	-Wall \
	-Werror \
	-W \
	-Wno-unused-parameter \
	-Wnon-virtual-dtor \
	-pthread \
	-fno-rtti \
	-fno-exceptions \
	-pedantic \
	-ansi \
	-fvisibility=hidden \
	-g \
	-O0 \
	-Wall \
	-Werror \
	-W \
	-Wno-unused-parameter \
	-Wnon-virtual-dtor \
	-Woverloaded-virtual

# Flags passed to only C files.
CFLAGS_C_Debug :=

# Flags passed to only C++ files.
CFLAGS_CC_Debug :=

INCS_Debug := \
	-I$(srcdir)/src

DEFS_Release := \
	'-DENABLE_DEBUGGER_SUPPORT' \
	'-DV8_TARGET_ARCH_X64'

# Flags passed to all source files.
CFLAGS_Release := \
	-Wall \
	-Werror \
	-W \
	-Wno-unused-parameter \
	-Wnon-virtual-dtor \
	-pthread \
	-fno-rtti \
	-fno-exceptions \
	-pedantic \
	-ansi \
	-fvisibility=hidden \
	-fdata-sections \
	-ffunction-sections \
	-fno-delete-null-pointer-checks \
	-O3

# Flags passed to only C files.
CFLAGS_C_Release :=

# Flags passed to only C++ files.
CFLAGS_CC_Release :=

INCS_Release := \
	-I$(srcdir)/src

OBJS := \
	$(obj).target/$(TARGET)/src/allocation.o \
	$(obj).target/$(TARGET)/src/atomicops_internals_x86_gcc.o \
	$(obj).target/$(TARGET)/src/bignum.o \
	$(obj).target/$(TARGET)/src/bignum-dtoa.o \
	$(obj).target/$(TARGET)/src/cached-powers.o \
	$(obj).target/$(TARGET)/src/conversions.o \
	$(obj).target/$(TARGET)/src/diy-fp.o \
	$(obj).target/$(TARGET)/src/dtoa.o \
	$(obj).target/$(TARGET)/src/fast-dtoa.o \
	$(obj).target/$(TARGET)/src/fixed-dtoa.o \
	$(obj).target/$(TARGET)/src/once.o \
	$(obj).target/$(TARGET)/src/preparse-data.o \
	$(obj).target/$(TARGET)/src/preparser.o \
	$(obj).target/$(TARGET)/src/preparser-api.o \
	$(obj).target/$(TARGET)/src/scanner.o \
	$(obj).target/$(TARGET)/src/strtod.o \
	$(obj).target/$(TARGET)/src/token.o \
	$(obj).target/$(TARGET)/src/unicode.o \
	$(obj).target/$(TARGET)/src/utils.o

# Add to the list of files we specially track dependencies for.
all_deps += $(OBJS)

# CFLAGS et al overrides must be target-local.
# See "Target-specific Variable Values" in the GNU Make manual.
$(OBJS): TOOLSET := $(TOOLSET)
$(OBJS): GYP_CFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))  $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_C_$(BUILDTYPE))
$(OBJS): GYP_CXXFLAGS := $(DEFS_$(BUILDTYPE)) $(INCS_$(BUILDTYPE))  $(CFLAGS_$(BUILDTYPE)) $(CFLAGS_CC_$(BUILDTYPE))

# Suffix rules, putting all outputs into $(obj).

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(srcdir)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# Try building from generated source, too.

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj).$(TOOLSET)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

$(obj).$(TOOLSET)/$(TARGET)/%.o: $(obj)/%.cc FORCE_DO_CMD
	@$(call do_cmd,cxx,1)

# End of this set of suffix rules
### Rules for final target.
LDFLAGS_Debug := \
	-pthread

LDFLAGS_Release := \
	-pthread

LIBS :=

$(obj).target/tools/gyp/libpreparser_lib.a: GYP_LDFLAGS := $(LDFLAGS_$(BUILDTYPE))
$(obj).target/tools/gyp/libpreparser_lib.a: LIBS := $(LIBS)
$(obj).target/tools/gyp/libpreparser_lib.a: TOOLSET := $(TOOLSET)
$(obj).target/tools/gyp/libpreparser_lib.a: $(OBJS) FORCE_DO_CMD
	$(call do_cmd,alink)

all_deps += $(obj).target/tools/gyp/libpreparser_lib.a
# Add target alias
.PHONY: preparser_lib
preparser_lib: $(obj).target/tools/gyp/libpreparser_lib.a

# Add target alias to "all" target.
.PHONY: all
all: preparser_lib

