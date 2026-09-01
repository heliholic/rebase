###############################################################################
# Generate pg_hash.h from DWARF layouts of PG_DECLARE types.
#
# Output lands in $(TARGET_OBJ_DIR)/pg_hash/, never the source tree: the
# layouts are TARGET-specific, so a shared path would race under a parallel
# multi-target build. PG_HASH_DIR goes on the include path, so firmware can
# #include "pg_hash.h" and pick up its own target's copy.
#
# Compiles a throwaway TU with the same TARGET CFLAGS (plus -g / -fno-lto),
# scans PG_DECLARE for type names, then extracts the DWARF layout.
# pg_hash.h has a PG_<NAME>_HASH FNV-1 define per PG.
#
# pg-hash builds ONE TU including every pg/*.h. pg-defs builds one TU per
# header, which is far slower but yields a readable per-header .def and
# isolates a header that fails to compile.
#
#   make pg-defs                  # default TARGET (STM32F722)
#   make pg-defs TARGET=STM32H743
#   make pg-hash                  # <obj>/pg_hash/pg_hash.h
#
###############################################################################

PG_DUMP_SCRIPT  := $(ROOT)/src/utils/pg_dump_structs.py
PG_HEADER_DIR   := $(SRC_DIR)/pg
PG_HEADERS      := $(shell grep -l 'PG_DECLARE' $(PG_HEADER_DIR)/*.h)

PG_HASH_DIR     := $(TARGET_OBJ_DIR)/pg_hash
PG_HASH_HEADER  := $(PG_HASH_DIR)/pg_hash.h

PG_DEFS_DIR     := $(TARGET_OBJ_DIR)/pg_defs
PG_DEFS_FILES   := $(patsubst $(PG_HEADER_DIR)/%.h,$(PG_DEFS_DIR)/%.def,$(PG_HEADERS))

# This file is included after CFLAGS has been assembled, so extend CFLAGS
# rather than INCLUDE_DIRS. Recipes expand CFLAGS at rule time, so the -I
# still reaches every compile.
CFLAGS          += -I$(PG_HASH_DIR)


## Targets

.PHONY: pg-hash pg-defs pg-clean

ifeq ($(TARGET),)

pg-hash:
	$(V0) $(MAKE) TARGET=$(DEFAULT_TARGET) pg-hash

pg-defs:
	$(V0) $(MAKE) TARGET=$(DEFAULT_TARGET) pg-defs

pg-clean:
	$(V0) $(MAKE) TARGET=$(DEFAULT_TARGET) pg-clean

else

pg-hash: $(PG_HASH_HEADER)

pg-defs: $(PG_DEFS_FILES)

pg-clean:
	$(V0) rm -rf $(PG_HASH_DIR) $(PG_DEFS_DIR) $(PG_HASH_OBJ_DIR)


PG_HASH_OBJ_DIR  := $(TARGET_OBJ_DIR)/pg_probe

PG_HASH_CFLAGS   := $(filter-out -flto=auto -flto -fuse-linker-plugin -MMD -MP,$(CFLAGS)) \
                   -g3 -gdwarf-4 -fno-lto -fno-eliminate-unused-debug-types -O0 \
                   -Wno-unused-function -DPG_LAYOUT_PROBE

# pg.h includes pg_hash.h. Firmware objects cannot compile until it
# exists; gcc -MMD cannot name a header that is not yet on disk. Probe
# TUs skip that include via -DPG_LAYOUT_PROBE so they do not form a
# make cycle (pg_all.o -> pg_hash.h -> pg_all.o).
$(TARGET_OBJS): $(PG_HASH_HEADER)

PG_HASH_DEPS     := $(patsubst $(PG_HEADER_DIR)/%.h,$(PG_HASH_OBJ_DIR)/%.d,$(PG_HEADERS))

$(PG_HASH_DIR) $(PG_DEFS_DIR) $(PG_HASH_OBJ_DIR):
	$(V1) mkdir -p $@

# Compile a throwaway TU that includes the PG header with the same
# preprocessor environment as firmware. The production .c is not used:
# many PG sources wrap the types in USE_* ifdefs or pull reset-template
# code that is not needed to recover the layout.
$(PG_DEFS_DIR)/%.def: $(PG_HEADER_DIR)/%.h $(PG_DUMP_SCRIPT) | $(PG_HASH_OBJ_DIR) $(PG_DEFS_DIR)
	@echo "%% (pg-def) pg/$*.h" "$(STDOUT)"
	$(V1) printf '#include <stdbool.h>\n#include <stdint.h>\n#include "platform.h"\n#include "pg/%s.h"\n' $* > $(PG_HASH_OBJ_DIR)/$*.c
	$(V1) $(CROSS_CC) -c -o $(PG_HASH_OBJ_DIR)/$*.o $(PG_HASH_CFLAGS) \
		-MMD -MP -MT $@ -MF $(PG_HASH_OBJ_DIR)/$*.d $(PG_HASH_OBJ_DIR)/$*.c
	$(V1) $(PYTHON) $(PG_DUMP_SCRIPT) \
		--object $(PG_HASH_OBJ_DIR)/$*.o \
		--header $(PG_HEADER_DIR)/$*.h \
		--output $@ \
		--target $(TARGET)

# Array lengths and USE_* gating come from platform.h / target headers, so a
# .def is stale whenever anything the probe TU included has changed.
-include $(PG_HASH_DEPS)

# One TU with every pg/*.h. The .def files are not an input to the hash -
# only the headers (for PG_DECLARE) and the DWARF are - so building 70+
# separate objects here would be wasted work.
PG_ALL_SRC      := $(PG_HASH_OBJ_DIR)/pg_all.c
PG_ALL_OBJ      := $(PG_HASH_OBJ_DIR)/pg_all.o

$(PG_ALL_OBJ): $(PG_HEADERS) | $(PG_HASH_OBJ_DIR)
	@echo "%% (pg-hash) pg_all.c" "$(STDOUT)"
	$(V1) { printf '#include <stdbool.h>\n#include <stdint.h>\n#include "platform.h"\n'; \
		for h in $(notdir $(PG_HEADERS)); do printf '#include "pg/%s"\n' "$$h"; done; \
	} > $(PG_ALL_SRC)
	$(V1) $(CROSS_CC) -c -o $@ $(PG_HASH_CFLAGS) \
		-MMD -MP -MT $@ -MF $(PG_HASH_OBJ_DIR)/pg_all.d $(PG_ALL_SRC)

-include $(PG_HASH_OBJ_DIR)/pg_all.d

$(PG_HASH_HEADER): $(PG_ALL_OBJ) $(PG_DUMP_SCRIPT) | $(PG_HASH_DIR)
	@echo "%% (pg-hash) pg_hash.h" "$(STDOUT)"
	$(V1) $(PYTHON) $(PG_DUMP_SCRIPT) \
		--hash-header $@ \
		--object $(PG_ALL_OBJ) \
		--header-dir $(PG_HEADER_DIR) \
		--target $(TARGET)

endif
