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
# The null GL and the benchmark's words are here, so the checks cover them
# without the pak.
CORE   := lib/quince_core.c lib/selftest.c lib/nullgl.c lib/qbargs.c lib/pb3d.c
# Quake (M1): id's files, QUINCE's null GL and benchmark (lib/), the
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
CANNER_FLAGS ?=
# The .JAM that sim-smoke, sim-bench and send take: CANNER's plain build,
# or build/heat/ or build/gcc/'s (below).
QUINCE_JAM ?= $(BUILD)/$(NAME).JAM

.PHONY: FORCE all app heat gcc test quake-host test-quake sim-bench page-check sim-kernel sim-smoke sim send dist os pin clean
all: app
app: $(BUILD)/$(NAME).JAM

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
test: $(BUILD)/test_quince test-quake
	$(BUILD)/test_quince

$(BUILD)/test_quince: tests/test_quince.c $(CORE) $(wildcard lib/*.h) Makefile
	@mkdir -p $(BUILD)
	$(HOSTCC) -std=c11 -g -O1 -Wall -Wextra -Werror -I lib $(HOST_SAN) \
	    -o $@ tests/test_quince.c $(CORE) -lm

# Quake on the PC (M1): id's files in quake/ (as GLQuake builds them, with
# the GL renderer and the null sound, CD and input), QUINCE's null GL,
# video layer and benchmark driver from lib/, and tests/sys_host.c. Built
# 32-bit, because Quake keeps pointers in ints in places, and with id386
# off (-U__i386__), so it takes the C paths the board takes, and with
# enums as small as their values (-fshort-enums), as arm-none-eabi has them
# (GCC and CANNER alike: quake/modelgen.h says what that cost). id's files
# are old C: gnu89, common symbols, and their warnings left alone.
QHOST_CC  := $(HOSTCC) -m32 -U__i386__ -fshort-enums -DGLQUAKE -I lib -I quake -O2 -g
QHOST_OBJ := $(patsubst %.c,$(BUILD)/qhost/%.o,$(QUAKE_SRC) $(QUAKE_LIB) lib/nullgl.c lib/qbargs.c tests/sys_host.c)

$(BUILD)/qhost/quake/%.o: quake/%.c $(wildcard quake/*.h) lib/GL/gl.h Makefile
	@mkdir -p $(dir $@)
	$(QHOST_CC) -std=gnu89 -fcommon -w -c -o $@ $<
$(BUILD)/qhost/%.o: %.c $(wildcard quake/*.h lib/*.h) lib/GL/gl.h Makefile
	@mkdir -p $(dir $@)
	$(QHOST_CC) -std=gnu99 -fcommon -Wall -Wextra -Wno-unknown-pragmas \
	    -Wno-unused-parameter -Wno-sign-compare -Werror -c -o $@ $<
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
# loads. The times are printed, not checked.
QUINCE_PAK ?=
quake-host: $(BUILD)/quake_host
test-quake:
	@if [ -z "$(QUINCE_PAK)" ] || [ ! -f "$(QUINCE_PAK)" ]; then \
	    echo "test-quake: skipped (set QUINCE_PAK to the shareware id1/pak0.pak)"; exit 0; fi; \
	if ! printf '#include <stdio.h>\nint main(void) { return 0; }\n' | \
	    $(HOSTCC) -m32 -x c -o /dev/null - 2>/dev/null; then \
	    echo "test-quake: skipped ($(HOSTCC) -m32 can't build: no 32-bit C library; gcc-multilib on Debian and Ubuntu)"; exit 0; fi; \
	$(MAKE) --no-print-directory $(BUILD)/quake_host && \
	mkdir -p $(BUILD)/qdata/id1 && ln -sf "$(abspath $(QUINCE_PAK))" $(BUILD)/qdata/id1/pak0.pak && \
	$(PYTHON) tests/test_quake.py $(BUILD)/quake_host $(BUILD)/qdata

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
include $(SDK)/pectin.mk
GCC_DIR   := $(BUILD)/gcc/$(NAME)
GCC_QSRC  := $(SRC) $(CORE) $(QUAKE_BOARD) $(SDKLIB)
GCC_QOBJ  := $(patsubst %.c,$(GCC_DIR)/obj/%.o,$(GCC_QSRC))
GCC_CFLAGS := $(APP_ARCH) -std=gnu99 -ffreestanding -nostdlib -O2 -g -w \
              -Werror=implicit-function-declaration -fno-strict-aliasing -fcommon \
              -ffunction-sections -fdata-sections -ffp-contract=off -fno-math-errno \
              -fno-tree-loop-distribute-patterns $(QUAKE_INC) -I lib -I $(SDK)
gcc: $(BUILD)/gcc/$(NAME).JAM
$(GCC_DIR)/obj/%.o: %.c $(wildcard lib/*.h quake/*.h quince/libc/*.h) $(SDK)/pectin.h Makefile
	@mkdir -p $(dir $@)
	$(CROSS)gcc $(GCC_CFLAGS) -c -o $@ $<
$(GCC_DIR)/$(NAME).elf: $(GCC_QOBJ) $(APP_LIBSRC) $(SDK)/app.ld $(SDK)/cold.ld
	$(CROSS)gcc $(APP_CFLAGS) -O2 -fno-tree-loop-distribute-patterns $(APP_LDFLAGS) \
	    -Wl,-Map=$(GCC_DIR)/$(NAME).map -o $@ $(GCC_QOBJ) $(APP_LIBSRC)
	@if grep -q 'libgcc\.a' $(GCC_DIR)/$(NAME).map; then \
	    echo "error: $@ links libgcc (pectinos has none: its CLAUDE.md, #112)"; rm -f $@; exit 1; fi
	$(CROSS)objdump -d -S $@ > $(GCC_DIR)/$(NAME).lst
$(BUILD)/gcc/$(NAME).JAM: $(GCC_DIR)/$(NAME).elf $(PAGE) $(VERB) $(PRF) $(VERBUSAGE) $(PREFCHECK)
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
