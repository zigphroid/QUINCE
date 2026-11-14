# QUINCE: Quake, ported to PectinOS.
#
# An out-of-tree PectinOS program: everything builds against the pectinos
# checkout PECTIN_ROOT names, which owns the SDK, CANNER, SIMMER and the
# tools. Written by pectinos's `make new-app` at bafa5bf
# ($PECTIN_ROOT/sdk/README.md, "Building outside the tree", is the recipe).

# ~/pectinos, else a sibling ../pectinos (where a cloud session keeps
# it: the start hook's clone, or a Claude project's own pectinos checkout).
PECTIN_ROOT ?= $(if $(wildcard $(HOME)/pectinos/.git),$(HOME)/pectinos,$(if $(wildcard ../pectinos/.git),$(abspath ../pectinos),$(HOME)/pectinos))
SDK        := $(PECTIN_ROOT)/sdk
CANNER     := $(PECTIN_ROOT)/build/host/canner
VERBUSAGE  := $(PECTIN_ROOT)/build/host/verbusage
PREFCHECK  := $(PECTIN_ROOT)/build/host/prefcheck
JARCARD    := $(PECTIN_ROOT)/build/host/jarcard
MKPAGES    := $(PECTIN_ROOT)/tools/mkpages.py
QEMU       := $(PECTIN_ROOT)/simmer/qemu/bin/qemu-system-arm
SIMKERNEL  := $(PECTIN_ROOT)/build/sim/pectinos.elf
PYTHON     ?= python3
HOSTCC     ?= cc
BUILD      := build

# PECTIN_REV pins the pectinos commit QUINCE is known to build and pass
# against. The board must run PectinOS from the same commit: a kernel older
# than the CANNER that built the .JAM refuses it ("Not a jar. (needs a
# newer PectinOS)"), so `make os` loads the pinned kernel. A checkout
# elsewhere still builds, with a warning; `make pin` moves the pin to the
# checkout's HEAD once QUINCE passes there.
# make splits paths at spaces, so a checkout or a repo in one can't build.
ifneq ($(word 2,$(PECTIN_ROOT)),)
$(error PECTIN_ROOT has a space in it ($(PECTIN_ROOT)), and make can't build in one: move the pectinos checkout to a path without spaces)
endif
ifneq ($(word 2,$(CURDIR)),)
$(error this repo is in a path with a space ($(CURDIR)), and make can't build in one: move it to a path without spaces)
endif
PECTIN_REV  := $(shell cat PECTIN_REV 2>/dev/null)
PECTIN_HEAD := $(shell git -C $(PECTIN_ROOT) rev-parse HEAD 2>/dev/null)
ifeq ($(PECTIN_HEAD),)
$(error no pectinos checkout at $(PECTIN_ROOT): clone https://github.com/zigphroid/pectinos there, or set PECTIN_ROOT)
endif
ifneq ($(PECTIN_REV),)
ifneq ($(PECTIN_HEAD),$(PECTIN_REV))
$(warning pectinos is at $(shell git -C $(PECTIN_ROOT) rev-parse --short HEAD), PECTIN_REV pins $(shell git -C $(PECTIN_ROOT) rev-parse --short $(PECTIN_REV) 2>/dev/null || echo $(PECTIN_REV)) (git -C $(PECTIN_ROOT) checkout $(PECTIN_REV), or make pin))
endif
endif

NAME   := QUINCE
SRC    := quince/$(NAME).c
# The core and the checks: no kernel calls, so they build on the host too.
# The GL layer to PB, its commands and the benchmark's words are here, so
# the checks cover them without the pak.
CORE   := lib/quince_core.c lib/selftest.c lib/pbgl.c lib/qbargs.c lib/pb3d.c
# Quake (M1): id's files, QUINCE's GL layer and benchmark (lib/), the
# board's system layer and the C library pieces the SDK hasn't got
# (quince/). The PC build below takes the same QUAKE_SRC and QUAKE_LIB.
QUAKE_SRC := $(addprefix quake/,cd_null.c chase.c cl_demo.c cl_input.c cl_main.c \
    cl_parse.c cl_tent.c cmd.c common.c console.c crc.c cvar.c host.c host_cmd.c \
    in_null.c keys.c mathlib.c menu.c net_loop.c net_main.c net_none.c pr_cmds.c \
    pr_edict.c pr_exec.c r_part.c sbar.c snd_null.c sv_main.c sv_move.c sv_phys.c \
    sv_user.c view.c wad.c world.c zone.c gl_draw.c gl_mesh.c gl_model.c \
    gl_refrag.c gl_rlight.c gl_rmain.c gl_rmisc.c gl_rsurf.c gl_screen.c gl_warp.c)
QUAKE_LIB := lib/qvid.c lib/qbench.c lib/qrand.c
QUAKE_BOARD := $(QUAKE_SRC) $(QUAKE_LIB) quince/sys_quince.c quince/qlibc.c
# quince/libc/ first: id's files include <stdio.h> and the rest, which
# PectinOS hasn't got as such (quince/libc/stdio.h says why).
QUAKE_INC := -I quince/libc -I quake -DGLQUAKE
PAGE   := quince/$(NAME).md
VERB   := quince/$(NAME).VRB
# Settings (sdk/README.md, "Settings"): a QUINCE.PRF beside the source, if
# the program has any, goes in as the PREF resource.
PRF    := $(wildcard quince/$(NAME).PRF)

# The SDK's C library, as the checkout's root Makefile lists it in
# CANNER_LIBSRC: one CANNER run compiles the program and the library
# together. If the checkout's list changes, this one follows it
# ($PECTIN_ROOT/apps/TEMPLATE/Makefile is kept to it by pectinos's tests).
LIB    := $(SDK)/lib/pectin.c \
          $(PECTIN_ROOT)/kernel/lib/string.c $(PECTIN_ROOT)/kernel/lib/format.c \
          $(PECTIN_ROOT)/kernel/lib/softfp.c $(PECTIN_ROOT)/kernel/lib/fmtfp.c \
          $(PECTIN_ROOT)/kernel/lib/readfp.c $(SDK)/lib/math.c
# The rest of the SDK's library the program uses: $(SDK)/lib/ui.c for
# ui.h's field, list and questions, http.c and json.c for theirs, and so
# on. Only what's used, since CANNER keeps every function it compiles.
SDKLIB :=
# The loader's default stack is 8 KB; 16 KB is the most there is. Big
# buffers go in static or on the heap, never on the stack.
STACK  := 16384
# The largest program span HEADSPACE gives on the board at the pin, with
# nothing resident: the 384 KB program area less the shell's stack (M4's
# board run, LOG 2026-11-12). make gcc holds GCC's build to it: code,
# data, SRAM .bss, the stack, the loader's 1 KB guard and the start
# block, the program's arguments and environment at the span's top
# (about 192 bytes for the board run's commands; 512 leaves room for a
# longer line or a bigger environment).
PROGRAM_SPAN := 384768
START_BLOCK  := 512
# The room make gcc's check keeps spare below that: the Mac's GCC writes
# about 2.8 KB more code than the cloud's (LOG 2026-11-14), so a build
# that fits here with less could be Headspace exceeded there (#57).
GCC_SRAM_MARGIN := 4096
CANNER_FLAGS ?=
# The .JAM that sim-smoke, sim-bench and send take: CANNER's plain build,
# or build/heat/ or build/gcc/'s (below).
QUINCE_JAM ?= $(BUILD)/$(NAME).JAM

.PHONY: FORCE all app heat gcc test test-builds quake-host test-quake test-builtins test-float pb-frames sim-bench page-check sim-kernel sim-smoke sim send dist os pin clean
all: app $(BUILD)/pbmodel.jo $(BUILD)/pbdecode.jo
app: $(BUILD)/$(NAME).JAM

# The triangle engine's C model isn't in the .JAM (PB draws, not the
# Jam), but it keeps to CANNER's subset like all of lib/, so pectinos can
# take it into its own tools: CANNER compiles it and its decoder alone,
# objects no one links.
$(BUILD)/pbmodel.jo: lib/pbmodel.c lib/pbmodel.h $(CANNER) Makefile
	@mkdir -p $(BUILD)
	$(CANNER) -I lib -I $(SDK) -c -o $@ lib/pbmodel.c
$(BUILD)/pbdecode.jo: lib/pbdecode.c lib/pbdecode.h lib/pbmodel.h lib/pb3d.h $(CANNER) Makefile
	@mkdir -p $(BUILD)
	$(CANNER) -I lib -I $(SDK) -c -o $@ lib/pbdecode.c

# pectinos's own rules decide whether its tools are current, so a checkout
# moved to a new pin rebuilds them before they build QUINCE. FORCE only
# asks: the .JAM rebuilds when a tool's file actually changed.
$(CANNER): FORCE
	@$(MAKE) -s --no-print-directory -C $(PECTIN_ROOT) canner
$(VERBUSAGE) $(PREFCHECK) $(JARCARD): FORCE
	@$(MAKE) -s --no-print-directory -C $(PECTIN_ROOT) build/host/$(notdir $@)
FORCE:

# CANNER stores the template, the settings and the page without reading
# them, so the checkout's own tools check them first: verbusage -f refuses
# a template the shell couldn't parse, prefcheck a settings description,
# and mkpages.py --check a page COOKBOOK would show wrongly.
$(BUILD)/$(NAME).JAM: $(SRC) $(CORE) $(QUAKE_BOARD) $(wildcard lib/*.h quake/*.h quince/libc/*.h) \
                      $(PAGE) $(VERB) $(PRF) \
                      $(LIB) $(SDKLIB) $(SDK)/pectin.h $(CANNER) $(VERBUSAGE) $(PREFCHECK) Makefile
	@mkdir -p $(BUILD)
	$(VERBUSAGE) -f $(VERB) > /dev/null
	$(if $(PRF),$(PREFCHECK) $(PRF) > /dev/null)
	$(PYTHON) $(MKPAGES) --check $(PAGE)
	$(CANNER) $(QUAKE_INC) -I $(SDK) -I lib $(CANNER_FLAGS) -stack $(STACK) -page $(PAGE) -verb $(VERB) \
	    $(if $(PRF),-pref $(PRF)) -o $@ $(SRC) $(CORE) $(QUAKE_BOARD) $(LIB) $(SDKLIB)

page-check:
	$(PYTHON) $(MKPAGES) --check $(PAGE)

# The host suite, under the sanitizers: the check to run after every
# change. lib/selftest.c's checks are the same ones `QUINCE /SELFTEST`
# runs on the board; host-only ones go in tests/.
HOST_SAN ?= -fsanitize=address,undefined -fno-sanitize-recover=undefined \
            -fno-omit-frame-pointer
test: $(BUILD)/test_quince $(BUILD)/test_pbmodel test-quake test-builtins test-float test-builds
	$(BUILD)/test_quince
	$(PYTHON) tests/pbcheck.py --self-test
	@mkdir -p $(BUILD)/pbmodel
	$(BUILD)/test_pbmodel $(BUILD)/pbmodel/scene.png

# make heat and make gcc are only built by hand otherwise, so a source
# left out of GCC_QSRC, a double function quince/libc/ lacks, or a heat
# cut that no longer builds would wait for the next board comparison to
# show (#11). A heat file that names functions gone from the build only
# warns: heat.py's warnings say which, and the cut still builds. Built
# here, not run: sim-smoke and sim-bench take QUINCE_JAM= for that. The
# heat cut needs only CANNER; GCC's build skips, saying why, without
# $(CROSS)gcc. Each build's output goes to a log in $(BUILD), printed
# only if it fails (#42).
test-builds:
	@mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory heat > $(BUILD)/test-heat.log 2>&1 || \
	    { cat $(BUILD)/test-heat.log; echo "test-builds: FAIL: make heat"; exit 1; }
	@if command -v $(CROSS)gcc > /dev/null 2>&1; then \
	    $(MAKE) --no-print-directory gcc > $(BUILD)/test-gcc.log 2>&1 || \
	    { cat $(BUILD)/test-gcc.log; echo "test-builds: FAIL: make gcc"; exit 1; }; \
	else echo "test-builds: make gcc skipped: no $(CROSS)gcc"; fi
	@echo "test-builds: make heat$(if $(shell command -v $(CROSS)gcc 2>/dev/null), and make gcc,) built"

# Quake's hot functions stay float (M3 PR 2, LOG 2026-11-12): CANNER writes
# the whole program as assembly, and tools/floatcheck.py fails if a
# function tests/float_hot.txt names calls softfp's double helpers or a
# double math.h function more often than the list allows.
test-float: $(SRC) $(CORE) $(QUAKE_BOARD) $(wildcard lib/*.h quake/*.h quince/libc/*.h) \
            tests/float_hot.txt tools/floatcheck.py $(CANNER) Makefile
	@mkdir -p $(BUILD)
	$(CANNER) $(QUAKE_INC) -I $(SDK) -I lib $(CANNER_FLAGS) -S \
	    $(SRC) $(CORE) $(QUAKE_BOARD) $(LIB) $(SDKLIB) > $(BUILD)/quince.s
	$(PYTHON) tools/floatcheck.py $(BUILD)/quince.s tests/float_hot.txt

# quince/libc/math.h's FPU built-ins (M3, LOG 2026-11-12): CANNER compiles
# tests/fpu_builtins.c through it, with the SDK's math.c beside it as in
# the real build, and any q_ function that calls sqrtf, fabsf, floorf,
# ceilf or __builtin_roundf (lib/pb3d.c's) instead of making the
# instruction fails it. The state and checksum can't catch that: math.c's
# sqrtf is correctly rounded too, only slower.
test-builtins: tests/fpu_builtins.c quince/libc/math.h $(CANNER) Makefile
	@mkdir -p $(BUILD)
	$(CANNER) -I quince/libc -I $(SDK) -S tests/fpu_builtins.c $(filter %/math.c %/softfp.c,$(LIB)) \
	    > $(BUILD)/fpu_builtins.s
	@awk '/^[A-Za-z_][A-Za-z0-9_.]*:$$/ { fn = substr($$1, 1, length($$1) - 1) } \
	    fn ~ /^q_/ && /\tb(l|\.w)?\t/ { print "test-builtins: FAIL: " fn " calls " $$2; bad = 1 } \
	    END { exit bad }' $(BUILD)/fpu_builtins.s
	@echo "test-builtins: sqrtf, fabsf, floorf, ceilf and roundf are FPU instructions"

$(BUILD)/test_quince: tests/test_quince.c $(CORE) $(wildcard lib/*.h) Makefile
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c11 -g -O1 -Wall -Wextra -Werror -I lib $(HOST_SAN) \
	    -o $@ tests/test_quince.c $(CORE) -lm

# The triangle engine's C model (lib/pbmodel.c, docs/PB3D.md) held to its
# rules, and its decoder of lib/pb3d.h's words (lib/pbdecode.c), and a
# scene drawn with all of them: build/pbmodel/scene.png.
PBMODEL := lib/pbmodel.c lib/pbdecode.c lib/pb3d.c
PBMODEL_H := lib/pbmodel.h lib/pbdecode.h lib/pb3d.h
$(BUILD)/test_pbmodel: tests/test_pbmodel.c tests/png.c tests/png.h $(PBMODEL) $(PBMODEL_H) Makefile
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c11 -g -O1 -Wall -Wextra -Werror -I lib $(HOST_SAN) \
	    -o $@ tests/test_pbmodel.c tests/png.c $(PBMODEL) -lm

# A QUINCE_PBDUMP file drawn by the model, the way PB would draw it
# (tests/pbplay.c). Optimised and without the sanitizers, since demo1 is
# a thousand frames: test_pbmodel runs the same code under them.
$(BUILD)/pbplay: tests/pbplay.c tests/png.c tests/png.h $(PBMODEL) $(PBMODEL_H) Makefile
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c11 -O2 -g -Wall -Wextra -Werror -I lib -o $@ tests/pbplay.c tests/png.c $(PBMODEL) -lm

# Quake on the PC (M1): id's files in quake/ (as GLQuake builds them, with
# the GL renderer and the null sound, CD and input), QUINCE's GL layer,
# video layer and benchmark driver from lib/, and tests/sys_host.c. Built
# 32-bit, because Quake keeps pointers in ints in places, and with id386
# off (-U__i386__), so it takes the C paths the board takes, and with
# enums as small as their values (-fshort-enums), as arm-none-eabi has them
# (GCC and CANNER alike: quake/modelgen.h says what that cost). id's files
# are old C: gnu89, common symbols, and their warnings left alone.
# Its floating point is the board's, bit for bit (M3, LOG 2026-11-12), so
# the GL layer's commands come out the same word for word (M2 PR 2: the
# checksum test-quake and sim-bench hold): SSE
# (-msse2 -mfpmath=sse) rounds each float and double operation to its own
# width, as the M33's FPU and softfp.c do, where -m32's default x87 keeps
# them in 80 bits until they are stored; nothing is fused into a
# multiply-add (-ffp-contract=off), as CANNER fuses nothing; and math.h's
# functions are the SDK's own (sdk/lib/math.c, with softfp.c's packing),
# linked ahead of -lm, so sinf and the rest give the board's last bit too
# (but asin, which only mirrors call, and ceil, from QuakeC's builtin and
# gl_model.c's surface extents: exact, so glibc's can't differ; the board
# takes both from qlibc.c).
# -fno-builtin keeps GCC from swapping those calls for its own, as the
# board's -ffreestanding does: without it a sin and a cos of one angle
# became a call to glibc's sincos (AngleVectors and seven others).
QHOST_CC  := $(HOSTCC) -m32 -msse2 -mfpmath=sse -ffp-contract=off -fno-builtin -U__i386__ \
    -fshort-enums -DGLQUAKE -I lib -I quake -O2 -g
QHOST_OBJ := $(patsubst %.c,$(BUILD)/qhost/%.o,$(QUAKE_SRC) $(QUAKE_LIB) lib/pbgl.c lib/pb3d.c lib/qbargs.c tests/sys_host.c tests/pbdump.c) \
    $(BUILD)/qhost/sdk/math.o $(BUILD)/qhost/sdk/softfp.o

# SOURCE_DATE_EPOCH fixes __DATE__ and __TIME__, which host.c prints on
# the console, so the frames tests/pbplay.c draws are the same every build.
$(BUILD)/qhost/quake/%.o: quake/%.c $(wildcard quake/*.h) lib/GL/gl.h Makefile
	@mkdir -p $(dir $@)
	SOURCE_DATE_EPOCH=0 $(QHOST_CC) -std=gnu89 -fcommon -w -c -o $@ $<
$(BUILD)/qhost/%.o: %.c $(wildcard quake/*.h lib/*.h) lib/GL/gl.h Makefile
	@mkdir -p $(dir $@)
	$(QHOST_CC) -std=gnu99 -fcommon -Wall -Wextra -Wno-unknown-pragmas \
	    -Wno-unused-parameter -Wno-sign-compare -Werror -c -o $@ $<
$(BUILD)/qhost/sdk/math.o: $(SDK)/lib/math.c $(SDK)/math.h $(SDK)/lib/mathcoef.h Makefile
	@mkdir -p $(dir $@)
	$(QHOST_CC) -std=c11 -c -o $@ $<
$(BUILD)/qhost/sdk/softfp.o: $(PECTIN_ROOT)/kernel/lib/softfp.c $(PECTIN_ROOT)/kernel/lib/softfp.h Makefile
	@mkdir -p $(dir $@)
	$(QHOST_CC) -std=c11 -I $(PECTIN_ROOT)/kernel -c -o $@ $<
$(BUILD)/quake_host: $(QHOST_OBJ)
	$(QHOST_CC) -o $@ $^ -lm

# M1's two timings on the PC, against the shareware pak QUINCE_PAK names
# (never committed: Quake's game data comes from your own copy, README's
# "Licence"). Skipped, saying so, without one, and without a 32-bit C
# library to build quake_host with (-m32: gcc-multilib on Debian and
# Ubuntu, which the cloud session's start hook installs; Apple's clang
# has none), so `make test` passes on a fresh machine either way.
# Checked: demo1 is 969 frames, the server benchmark's fixed steps send
# the same triangles and vertices every run, and every shareware map
# loads, and e1m1's dumped frames, drawn by the triangle engine's C model
# (tests/pbplay.c), keep their pictures' CRCs. The times are printed, not
# checked.
QUINCE_PAK ?=
quake-host: $(BUILD)/quake_host
test-quake:
	@if [ -z "$(QUINCE_PAK)" ] || [ ! -f "$(QUINCE_PAK)" ]; then \
	    echo "test-quake: skipped (set QUINCE_PAK to the shareware id1/pak0.pak)"; exit 0; fi; \
	if ! printf '#include <stdio.h>\nint main(void) { return 0; }\n' | \
	    $(HOSTCC) -m32 -x c -o /dev/null - 2>/dev/null; then \
	    echo "test-quake: skipped ($(HOSTCC) -m32 can't build: no 32-bit C library; gcc-multilib on Debian and Ubuntu)"; exit 0; fi; \
	$(MAKE) --no-print-directory $(BUILD)/quake_host $(BUILD)/pbplay && \
	mkdir -p $(BUILD)/qdata/id1 && ln -sf "$(abspath $(QUINCE_PAK))" $(BUILD)/qdata/id1/pak0.pak && \
	$(PYTHON) tests/test_quake.py $(BUILD)/quake_host $(BUILD)/qdata $(BUILD)/pbplay

# demo1 drawn by the triangle engine's C model: the dump written on a fixed
# clock (tests/sys_host.c), so the same every run, then every frame drawn
# in turn and a few written out, build/pbframes/demo1/frameNNNN.png, with
# their CRCs and the fill numbers. Not in make test: about 40 seconds.
PB_FRAMES ?= 100 300 500 700 900
pb-frames:
	@test -f "$(QUINCE_PAK)" || { echo "pb-frames: set QUINCE_PAK to the shareware id1/pak0.pak"; exit 1; }
	$(MAKE) --no-print-directory $(BUILD)/quake_host $(BUILD)/pbplay
	mkdir -p $(BUILD)/qdata/id1 $(BUILD)/pbframes/demo1 && \
	    ln -sf "$(abspath $(QUINCE_PAK))" $(BUILD)/qdata/id1/pak0.pak
	QUINCE_PBDUMP=$(BUILD)/demo1.pb $(BUILD)/quake_host $(BUILD)/qdata timedemo DEMO1 > /dev/null
	$(BUILD)/pbplay $(BUILD)/demo1.pb -o $(BUILD)/pbframes/demo1 $(PB_FRAMES)

# The SIMMER kernel, brought up to date by pectinos's own rules on every
# SIMMER run (under a second when it is). Its QEMU is
# `make -C $(PECTIN_ROOT) sim-qemu`, once.
sim-kernel:
	@$(MAKE) -s --no-print-directory -C $(PECTIN_ROOT) sim-build

# PectinOS in QEMU with no window: QUINCE.JAM on a scratch card,
# /SELFTEST under CANNER's code, then the program itself, Esc, and the
# prompt back (tools/simsmoke.py, on pectinos's tools/simdrive.py). Run it
# after any change that reaches the board.
sim-smoke: $(QUINCE_JAM) $(JARCARD) sim-kernel
	$(PYTHON) tools/simsmoke.py --root $(PECTIN_ROOT) --app $< --work $(BUILD)/simsmoke

# M1's two timings on SIMMER, CANNER's build, against QUINCE_PAK
# (tools/simbench.py). Not in make test: it takes minutes, and the pak.
sim-bench: $(QUINCE_JAM) $(JARCARD) sim-kernel
	@test -f "$(QUINCE_PAK)" || { echo "sim-bench: set QUINCE_PAK to the shareware id1/pak0.pak"; exit 1; }
	$(PYTHON) tools/simbench.py --root $(PECTIN_ROOT) --app $< --pak "$(QUINCE_PAK)" --work $(BUILD)/simbench

# M1 PR 3's two other builds, timed on the board beside the plain one.
# Each is still QUINCE.JAM, in a directory of its own, so sim-smoke,
# sim-bench and send take one by QUINCE_JAM:
#   make heat && make sim-bench QUINCE_JAM=build/heat/QUINCE.JAM QUINCE_PAK=...
#
# heat: CANNER's build with the functions THERMO never saw busy moved to
# the cold segment (.JAM v4), which runs from PSRAM behind the XIP cache,
# so the jar keeps only QUINCE_HOT_BUDGET bytes of the hottest code
# ($(PECTIN_ROOT)/apps/canner/README.md, "Cold code"). The heat file is a
# board run of the plain build (quince/QUINCE.HOT: THERMO /HEAT on demo1
# and e1m1, added by heat.py sum), and the cut is heat.py's, against that
# build: same sources, so the same functions.
QUINCE_HOT_BUDGET ?= 64K
heat: $(BUILD)/heat/$(NAME).JAM
$(BUILD)/heat/$(NAME).cold: $(BUILD)/$(NAME).JAM quince/$(NAME).HOT $(PECTIN_ROOT)/tools/heat.py
	@mkdir -p $(BUILD)/heat
	$(PYTHON) $(PECTIN_ROOT)/tools/heat.py cold --jam $< --budget $(QUINCE_HOT_BUDGET) quince/$(NAME).HOT -o $@
$(BUILD)/heat/$(NAME).JAM: $(BUILD)/heat/$(NAME).cold $(BUILD)/$(NAME).JAM
	$(CANNER) $(QUAKE_INC) -I $(SDK) -I lib $(CANNER_FLAGS) -cold $< -stack $(STACK) -page $(PAGE) -verb $(VERB) \
	    $(if $(PRF),-pref $(PRF)) -o $@ $(SRC) $(CORE) $(QUAKE_BOARD) $(LIB) $(SDKLIB)

# gcc: the same program built by arm-none-eabi-gcc -O2, the SDK's GCC
# route ($(SDK)/pectin.mk: its flags, its library and its link), to see
# what CANNER's code costs. Not pectin.mk's pectin_app itself, which
# builds every source with one set of flags, the SDK's -I first: QUINCE's
# files need quince/libc/ first, as CANNER's build has it, or <math.h> is
# the SDK's, which hasn't got ceil or asin, and an undeclared double
# function is a call returning int (C89's implicit declaration; the
# -Werror below makes that an error). GCC's other warnings are off (-w):
# id's 1996 C sets off hundreds, and CANNER's build is the checked one.
# -fno-strict-aliasing because id's code reads floats through int
# pointers (LittleFloat and the rest), and -std=gnu99 for id's C89.
# The .elf, .map and disassembly are in build/gcc/QUINCE/.
# pectin.mk sets PECTIN_ROOT to $(SDK)/.., which would overwrite ours for
# every rule after it (#12): the same directory today, but not if SDK is
# pointed elsewhere, and a path compared as a string sees the difference.
QUINCE_PECTIN_ROOT := $(PECTIN_ROOT)
include $(SDK)/pectin.mk
PECTIN_ROOT := $(QUINCE_PECTIN_ROOT)
GCC_DIR   := $(BUILD)/gcc/$(NAME)
GCC_QSRC  := $(SRC) $(CORE) $(QUAKE_BOARD) $(SDKLIB)
GCC_QOBJ  := $(patsubst %.c,$(GCC_DIR)/obj/%.o,$(GCC_QSRC))
# GCC's build keeps all its code in SRAM, the program area, which it had
# outgrown (#57). Two files the timed runs never enter go to PSRAM
# instead, behind the XIP cache (.JAM v4's cold segment, $(SDK)/app.ld):
# the menus (quake/menu.c, 21 KB) and the selftest (lib/selftest.c,
# 15 KB). The list is every function their objects define, read from
# their symbol tables, so a new one goes cold without a word, less
# GCC_HOT_KEEP: M_Draw, which SCR_UpdateScreen calls every frame (it
# returns at once with no menu up; THERMO saw it once in a demo1 run).
# tools/heat2ld.py writes the list as cold.ld, which the link finds
# before the SDK's empty one (-L, as pectin.mk's pectin_cold_link does).
# Unlike make heat's cut, every function the demos run stays in SRAM, so
# GCC's timings stay comparable with the runs before (LOG 2026-11-14).
GCC_COLD_SRC := quake/menu.c lib/selftest.c
GCC_HOT_KEEP := M_Draw
GCC_CFLAGS := $(APP_ARCH) -std=gnu99 -ffreestanding -nostdlib -O2 -g -w \
              -Werror=implicit-function-declaration -fno-strict-aliasing -fcommon \
              -ffunction-sections -fdata-sections -ffp-contract=off -fno-math-errno \
              -fno-tree-loop-distribute-patterns $(QUAKE_INC) -I lib -I $(SDK)
gcc: $(BUILD)/gcc/$(NAME).JAM
$(GCC_DIR)/obj/%.o: %.c $(wildcard lib/*.h quake/*.h quince/libc/*.h) $(SDK)/pectin.h Makefile
	@mkdir -p $(dir $@)
	$(CROSS)gcc $(GCC_CFLAGS) -c -o $@ $<
# One FILE:NAME line a function (heat2ld.py's format); a clone
# (foo.constprop.0) by its function's name, which heat2ld.py's patterns
# cover.
$(GCC_DIR)/$(NAME).cold: $(patsubst %.c,$(GCC_DIR)/obj/%.o,$(GCC_COLD_SRC)) Makefile
	@set -e; for o in $(patsubst %.c,$(GCC_DIR)/obj/%.o,$(GCC_COLD_SRC)); do \
	    $(CROSS)nm --defined-only $$o | awk -v f=$$(basename $$o .o).c -v keep=' $(GCC_HOT_KEEP) ' \
	        '$$2 ~ /^[tT]$$/ { n = $$3; sub(/\..*/, "", n); if (index(keep, " " n " ") == 0) print f ":" n }'; \
	done | sort -u > $@
	@test -s $@ || { echo "error: $@ is empty: GCC_COLD_SRC's objects define no functions"; exit 1; }
$(GCC_DIR)/cold.ld: $(GCC_DIR)/$(NAME).cold $(PECTIN_ROOT)/tools/heat2ld.py
	$(PYTHON) $(PECTIN_ROOT)/tools/heat2ld.py --cold $< -o $@
$(GCC_DIR)/$(NAME).elf: $(GCC_QOBJ) $(APP_LIBSRC) $(SDK)/app.ld $(GCC_DIR)/cold.ld
	$(CROSS)gcc $(APP_CFLAGS) -O2 -fno-tree-loop-distribute-patterns -L$(GCC_DIR) $(APP_LDFLAGS) \
	    -Wl,-Map=$(GCC_DIR)/$(NAME).map -o $@ $(GCC_QOBJ) $(APP_LIBSRC)
	@if grep -q 'libgcc\.a' $(GCC_DIR)/$(NAME).map; then \
	    echo "error: $@ links libgcc (pectinos has none: its CLAUDE.md, #112)"; rm -f $@; exit 1; fi
	$(CROSS)objdump -d -S $@ > $(GCC_DIR)/$(NAME).lst
# The stack the .JAM was made with, so a new STACK makes it (and checks
# it) again.
$(GCC_DIR)/stack: FORCE
	@mkdir -p $(dir $@)
	@echo $(STACK) | cmp -s - $@ || echo $(STACK) > $@
# Laid out as the loader's top_down (kernel/loader/jam.c) does, from the
# span's top: the start block (rounded to 8), the stack under it with its
# start on 32, the 1 KB guard, and the program's own memory must end
# below that, its .bss's end rounded to 8. Taken from the .elf: SRAM
# starts at 0x20000000 and __bss_end ends SRAM's .bss.
$(BUILD)/gcc/$(NAME).JAM: $(GCC_DIR)/$(NAME).elf $(GCC_DIR)/stack $(PAGE) $(VERB) $(PRF) $(VERBUSAGE) $(PREFCHECK)
	@end=$$($(CROSS)nm $< | awk '$$3 == "__bss_end" { print $$1 }'); \
	mem_lo=$$(( (0x$$end - 0x20000000 + 7) & ~7 )); \
	block_lo=$$(( ($(PROGRAM_SPAN) - (($(START_BLOCK) + 7) & ~7)) & ~7 )); \
	stack_lo=$$(( (block_lo - (($(STACK) + 7) & ~7)) & ~31 )); \
	mem_hi=$$(( stack_lo - 1024 )); \
	if [ $$mem_lo -gt $$mem_hi ]; then \
	    echo "error: $< ends SRAM at $$mem_lo bytes, past $$mem_hi, what a $(STACK)-byte stack" \
	         "leaves of the board's $(PROGRAM_SPAN) (Headspace exceeded.)"; rm -f $@; exit 1; fi; \
	if [ $$(( mem_lo + $(GCC_SRAM_MARGIN) )) -gt $$mem_hi ]; then \
	    echo "error: $< ends SRAM at $$mem_lo bytes, $$(( mem_hi - mem_lo )) under $$mem_hi:" \
	         "less than GCC_SRAM_MARGIN's $(GCC_SRAM_MARGIN), which another GCC's bigger code needs (#57)"; \
	    rm -f $@; exit 1; fi
	$(VERBUSAGE) -f $(VERB) > /dev/null
	$(if $(PRF),$(PREFCHECK) $(PRF) > /dev/null)
	$(PYTHON) $(PECTIN_ROOT)/tools/mkjam.py --name $(NAME) --stack $(STACK) --page $(PAGE) \
	    --verb $(VERB) $(if $(PRF),--pref $(PRF)) $< $@


# The same, with the screen in a window and the serial console in this
# terminal (Ctrl+A X quits). The card is build/sim/card.img, with the
# newest QUINCE.JAM put on it each time.
sim: $(BUILD)/$(NAME).JAM $(JARCARD) sim-kernel
	@mkdir -p $(BUILD)/sim
	@test -f $(BUILD)/sim/card.img || $(JARCARD) $(BUILD)/sim/card.img new 300
	$(JARCARD) $(BUILD)/sim/card.img put $<
	$(MAKE) -C $(PECTIN_ROOT) sim SIM_CARD=$(abspath $(BUILD)/sim/card.img)

# To the board's card over its USB-C console (HARVEST, YMODEM), as
# pectinos's `make send` does. Close tio first: it steals the reads.
send: $(QUINCE_JAM)
	$(PYTHON) $(PECTIN_ROOT)/tools/jamterm.py --put $<

# For a board that fetches QUINCE over the WiFi instead (a cloud session
# can't reach the board's USB): the built .JAM and PICKJAM.BAT in dist/,
# committed and merged into main, where the board's PICK finds them at
# raw.githubusercontent.com. The repo must be public, and GitHub's raw
# files can lag a merge by a few minutes. It prints the board's lines:
# the first fetch is typed at the prompt, which takes one screen row (the
# line editor never wraps, #2097): https://github.com/... when it fits,
# else the bare github.com/... (plain http first, which PICK follows to
# https), with a warning that its first hop can be answered by anyone on
# the network.
dist: $(BUILD)/$(NAME).JAM
	@mkdir -p dist
	cp $< dist/$(NAME).JAM
	cp tools/PICKJAM.BAT dist/PICKJAM.BAT
	@repo=$$(git remote get-url origin 2>/dev/null | sed -E 's#^.*github\.com[:/]##; s#\.git$$##'); \
	    test -n "$$repo" || repo=OWNER/REPO; \
	    line="PICK https://github.com/$$repo/raw/main/dist/PICKJAM.BAT PICKJAM.BAT"; \
	    short=; if [ $${#line} -gt 71 ]; then \
	        short=1; line="PICK github.com/$$repo/raw/main/dist/PICKJAM.BAT PICKJAM.BAT"; fi; \
	    echo "dist: commit dist/ and merge it into main; then on the board, once:"; \
	    printf '  CD J:\\JAMS\n  %s\n' "$$line"; \
	    echo "and after each merge:"; \
	    printf '  PICKJAM %s $(NAME)\n' "$$repo"; \
	    if [ -n "$$short" ]; then \
	        echo "dist: the https:// line is too long for the prompt (71 characters after J:\\JAMS> on 80"; \
	        echo "  columns), so that one starts plain http, which anyone on the board's network could answer"; \
	        echo "  with a file of their own: type it only on a network you trust, and TASTE PICKJAM.BAT after."; \
	        if [ $${#line} -gt 71 ]; then \
	            echo "dist: even that is $${#line} characters: copy dist/PICKJAM.BAT onto the card another way"; \
	        fi; \
	    fi

# PectinOS itself, from the pinned commit, onto the board: hold BUTTON1,
# tap reset, let go (or `make -C $(PECTIN_ROOT) bootsel` while PectinOS
# runs), then `make os`. It needs picotool; dragging
# $(PECTIN_ROOT)/build/pectinos.uf2 onto the RP2350 drive does the same.
os:
	@test "$(PECTIN_HEAD)" = "$(PECTIN_REV)" || { \
	    echo "make os: pectinos is at $(PECTIN_HEAD), the pin is $(PECTIN_REV)."; \
	    echo "  git -C $(PECTIN_ROOT) checkout $(PECTIN_REV), or make pin to move the pin."; exit 1; }
	$(MAKE) -C $(PECTIN_ROOT) load

pin:
	git -C $(PECTIN_ROOT) rev-parse HEAD > PECTIN_REV
	@echo "PECTIN_REV now $$(cat PECTIN_REV): run make test, make sim-smoke and make os, then commit it in a PR of its own"

clean:
	rm -rf $(BUILD)

# Into the store (zigphroid/pectin-crates), for anyone's GROCER FETCH:
# `make crate` builds build/QUINCE.CRT (VERSION's version; larder/ and
# seeds/ if it has them), `make preflight` checks it as the store will,
# and `make submit` opens the store's pull request ($PECTIN_ROOT/sdk/crate.mk).
include $(PECTIN_ROOT)/sdk/crate.mk
