

## V                 : Set verbosity level based on the V= parameter
##                     V=0 Low
##                     V=1 High
export AT := @

# NOTE: --no-print-directory is passed via MAKEFLAGS rather than by exporting
# MAKE with extra arguments. GCC's lto-wrapper execvp()s $MAKE verbatim, and
# fails if it is anything but a bare command name.

ifndef V
export V0    :=
export V1    := $(AT)
export STDOUT   :=
MAKEFLAGS += --no-print-directory
else ifeq ($(V), 0)
export V0    := $(AT)
export V1    := $(AT)
export STDOUT:= "> /dev/null"
MAKEFLAGS += --no-print-directory
else ifeq ($(V), 1)
export V0    :=
export V1    :=
export STDOUT   :=
endif
