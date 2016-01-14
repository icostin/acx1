projects := acx1 linesel acx1stest acx1dtest

acx1_prod := slib dlib
acx1_cfg := release
acx1_csrc := common.c gnulinux.c mswin.c ucw8.c
acx1_chdr := acx1.h
acx1_ldflags := -lpthread
#
# xxx_cflags (1: prj, 2: prod, 3: cfg, 4: bld, 5: src)
acx1_cflags = -DACX1_TARGET='"$($4_target)"' -DACX1_CONFIG='"$3"' -DACX1_COMPILER='"$($4_compiler)"'
acx1_slib_cflags := -DACX1_STATIC
acx1_dlib_cflags := -DACX1_DLIB_BUILD


acx1stest_csrc := test.c
acx1stest_cfg := release
# xxx_cflags (1: prj, 2: prod, 3: cfg, 4: bld, 5: src)
acx1stest_cflags := -DACX1_STATIC
acx1stest_ldflags = -static -lacx1$($3_sfx)$($4_sfx) -lpthread
acx1stest_idep := acx1_slib

acx1dtest_csrc := test.c
acx1dtest_cfg := release
# xxx_cflags (1: prj, 2: prod, 3: cfg, 4: bld, 5: src)
acx1dtest_ldflags = -lacx1$($3_sfx)$($4_sfx) -lpthread
acx1dtest_idep := acx1_dlib

linesel_csrc := linesel.c
linesel_cfg := release
linesel_cflags :=
linesel_ldflags := -lacx1$($3_sfx)$($4_sfx) -lpthread
linesel_idep := acx1_dlib

include icobld.mk

