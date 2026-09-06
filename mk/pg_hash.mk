###############################################################################
# Generate pg_hash.h from DWARF layouts of PG_DECLARE types.
#
# Output lands in $(TARGET_OBJ_DIR)/pg_hash/, never the source tree: each
# target build generates its own, and a shared path would race under a
# parallel multi-target build. PG_HASH_DIR goes on the include path, so
# firmware can #include "pg_hash.h" and pick up its own target's copy.
#
# The contents are the same everywhere. Every pg/*.h declares its groups
# unconditionally, so the header carries a PG_<NAME>_HASH for every group in
# pg/ whether or not this build registers one, and a group's layout may not
# vary with the target or the build options - pg-hash-check enforces both
# across every CI target.
#
# Compiles a throwaway TU with the same TARGET CFLAGS (plus -g / -fno-lto),
# scans PG_DECLARE for type names, then extracts the DWARF layout.
#
# pg-hash builds ONE TU including every pg/*.h. pg-defs builds one TU per
# header, which is far slower but yields a readable per-header .def and
# isolates a header that fails to compile. A .def repeats the hash of each
# group it dumps, so the layout and the hash it produces can be read together.
#
#   make pg-defs                  # default TARGET (STM32F722)
#   make pg-defs TARGET=STM32H743
#   make pg-hash                  # <obj>/pg_hash/pg_hash.h
#
###############################################################################

PG_DUMP_SCRIPT  := $(ROOT)/src/utils/pg_dump_structs.py
PG_HEADER_DIR   := $(SRC_DIR)/pg
PG_HEADERS      := $(shell grep -l 'PG_DECLARE' $(PG_HEADER_DIR)/*.h)

# The PGN is half of a layout hash, and it comes from text rather than from
# DWARF: PG_REGISTER* in pg/*.c names the PGN macro, pg_ids.h gives it a
# number. Neither reaches the compiler-generated .d, so name them here. Without
# this a PGN change leaves the old hash on disk, and the FC then accepts an
# EEPROM record under the wrong identity.
PG_PGN_SOURCES  := $(wildcard $(PG_HEADER_DIR)/*.c) $(PG_HEADER_DIR)/pg_ids.h

PG_HASH_DIR     := $(TARGET_OBJ_DIR)/pg_hash
PG_HASH_HEADER  := $(PG_HASH_DIR)/pg_hash.h

PG_DEFS_DIR     := $(TARGET_OBJ_DIR)/pg_defs
PG_DEFS_FILES   := $(patsubst $(PG_HEADER_DIR)/%.h,$(PG_DEFS_DIR)/%.def,$(PG_HEADERS))

## The protocol document
#
# docs/pg-format.md describes the stored-config format: every group, every
# struct nested in one, every enum stored in one. It is checked in, so a
# layout change shows up as a diff in review.
#
# It must not depend on what you are building. A PG holds the same bytes on
# every board - pg-hash-check proves it - but which groups a build *declares*
# still varies, so the document is always generated from one pinned reference
# target with the gates below forced on. pg-docs ignores TARGET for that
# reason.
PG_DOC_MD       := $(ROOT)/docs/pg-format.md
PG_DOC_TARGET   := STM32F722

# This file is included after CFLAGS has been assembled, so extend CFLAGS
# rather than INCLUDE_DIRS. Recipes expand CFLAGS at rule time, so the -I
# still reaches every compile.
CFLAGS          += -I$(PG_HASH_DIR)


## Targets

.PHONY: pg-hash pg-defs pg-clean pg-hash-check pg-docs pg-docs-build pg-docs-check pg-docs-verify

## pg-hash           : generate the PG layout hashes for TARGET
## pg-defs           : dump readable per-header PG layouts for TARGET
## pg-docs           : regenerate docs/pg-format.md
#
# Always builds the reference target, whatever TARGET says, so the file is
# reproducible from any working tree.
pg-docs:
	$(V0) $(MAKE) TARGET=$(PG_DOC_TARGET) pg-docs-build

## pg-docs-check     : fail if docs/pg-format.md is out of date
#
# Regenerates to a scratch path and compares, rather than rewriting the file
# in place: a check that mutates the working tree hides what it was checking,
# and CI wants the diff, not a repaired tree.
pg-docs-check:
	$(V0) $(MAKE) TARGET=$(PG_DOC_TARGET) pg-docs-verify

ifeq ($(TARGET),)

pg-hash:
	$(V0) $(MAKE) TARGET=$(DEFAULT_TARGET) pg-hash

pg-defs:
	$(V0) $(MAKE) TARGET=$(DEFAULT_TARGET) pg-defs

pg-clean:
	$(V0) $(MAKE) TARGET=$(DEFAULT_TARGET) pg-clean

## pg-hash-check     : check the PG layout hashes agree across targets
#
# A group's layout must not depend on which target or which build options
# produced it, or the same board built two ways cannot read back its own
# stored config. This regenerates the hashes for every CI target and fails if
# any PG_<NAME>_HASH has two values.
#
# Groups a target does not declare simply have no define, so they are compared
# only where both sides have one. Whether a target *registers* a group is a
# separate matter: a build with no use for one may omit it, and its hash is
# then never looked up.
pg-hash-check:
	$(V0) for t in $(CI_TARGETS); do \
		echo "%% (pg-hash-check) $$t"; \
		$(MAKE) TARGET=$$t pg-hash $(STDOUT) || exit 1; \
	done
	$(V1) cat $(foreach t,$(CI_TARGETS),$(OBJECT_DIR)/$(t)/pg_hash/pg_hash.h) \
	    | awk '/^#define PG_[A-Z0-9_]*_HASH/ { print $$2, $$3 }' | sort -u \
	    | awk '{ if ($$1 == prev && $$2 != prevval) { \
	                 printf "%s differs: %s and %s\n", $$1, prevval, $$2; bad = 1 } \
	             prev = $$1; prevval = $$2 } \
	           END { if (bad) { print "PG layout hashes are not target-invariant"; exit 1 } \
	                 else print "PG layout hashes agree across $(words $(CI_TARGETS)) targets" }'

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
#
# The probe carries $(TARGET_BUILD_INPUTS) for the same reason the firmware
# objects do. The auto-generated .d covers every header the TU reached, but
# not a -D that only ever exists at the make level: a USE_* toggled in
# target.mk, an EXTRA_FLAGS, or the PG_HASH_CFLAGS below. Without these, a
# flag change rebuilds the firmware and leaves the hashes behind it, and a
# stale hash makes the FC accept a record whose layout no longer matches.
$(PG_DEFS_DIR)/%.def: $(PG_HEADER_DIR)/%.h $(PG_DUMP_SCRIPT) $(PG_PGN_SOURCES) $(TARGET_BUILD_INPUTS) | $(PG_HASH_OBJ_DIR) $(PG_DEFS_DIR)
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

$(PG_ALL_OBJ): $(PG_HEADERS) $(TARGET_BUILD_INPUTS) | $(PG_HASH_OBJ_DIR)
	@echo "%% (pg-hash) pg_all.c" "$(STDOUT)"
	$(V1) { printf '#include <stdbool.h>\n#include <stdint.h>\n#include "platform.h"\n'; \
		for h in $(notdir $(PG_HEADERS)); do printf '#include "pg/%s"\n' "$$h"; done; \
	} > $(PG_ALL_SRC)
	$(V1) $(CROSS_CC) -c -o $@ $(PG_HASH_CFLAGS) \
		-MMD -MP -MT $@ -MF $(PG_HASH_OBJ_DIR)/pg_all.d $(PG_ALL_SRC)

-include $(PG_HASH_OBJ_DIR)/pg_all.d

$(PG_HASH_HEADER): $(PG_ALL_OBJ) $(PG_DUMP_SCRIPT) $(PG_PGN_SOURCES) | $(PG_HASH_DIR)
	@echo "%% (pg-hash) pg_hash.h" "$(STDOUT)"
	$(V1) $(PYTHON) $(PG_DUMP_SCRIPT) \
		--hash-header $@ \
		--object $(PG_ALL_OBJ) \
		--header-dir $(PG_HEADER_DIR) \
		--target $(TARGET)

# The document reads the same object as pg_hash.h: one probe, one set of
# layouts, so the file cannot describe a group differently from the header
# the firmware compiles against.
#
# Phony rather than a rule for $(PG_DOC_MD): "make pg-docs" is an explicit
# instruction to regenerate, and a file rule would decline to rebuild a
# hand-edited document whose timestamp is newer than the probe's.
pg-docs-build: $(PG_ALL_OBJ) $(PG_DUMP_SCRIPT) $(PG_PGN_SOURCES)
	@echo "%% (pg-docs) $(notdir $(PG_DOC_MD))" "$(STDOUT)"
	$(V1) $(PYTHON) $(PG_DUMP_SCRIPT) \
		--markdown $(PG_DOC_MD) \
		--object $(PG_ALL_OBJ) \
		--header-dir $(PG_HEADER_DIR)

PG_DOC_MD_TMP   := $(TARGET_OBJ_DIR)/pg_docs/pg-format.md

pg-docs-verify: $(PG_ALL_OBJ) $(PG_DUMP_SCRIPT) $(PG_PGN_SOURCES)
	@echo "%% (pg-docs-check) $(notdir $(PG_DOC_MD))" "$(STDOUT)"
	$(V1) mkdir -p $(dir $(PG_DOC_MD_TMP))
	$(V1) $(PYTHON) $(PG_DUMP_SCRIPT) \
		--markdown $(PG_DOC_MD_TMP) \
		--object $(PG_ALL_OBJ) \
		--header-dir $(PG_HEADER_DIR)
	$(V1) diff -u $(PG_DOC_MD) $(PG_DOC_MD_TMP) || { \
		echo "docs/pg-format.md is out of date - run 'make pg-docs' and commit the result"; \
		exit 1; }
	$(V1) echo "docs/pg-format.md is up to date"

endif
