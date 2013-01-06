# acx1 - Application Console Interface - ver. 1
# 
# makefile for Unix-like platforms
#
# Changelog:
#	* 2013/01/06 Costin Ionescu: initial release

ifeq ($(PREFIX_DIR),)
PREFIX_DIR:=$(HOME)/.local
endif

.PHONY: clean install dlib test linesel arc

gcc_flags := -fvisibility=hidden -Wall -Wextra -Werror -Iinclude

dlib: out/libacx1.so 

clean:
	-rm -rf out tags

arc:
	cd .. && tar -Jcvf acx1.txz acx1/src acx1/include acx1/makefile acx1/vs9make.cmd acx1/*.txt

tags:
	ctags -R --fields=+iaS --extra=+q --exclude='.git' .

test: dlib out/test
	LD_LIBRARY_PATH=out out/test 2> out/test.log

out:
	mkdir -p $@

install: out/libacx1.so out/linesel
	mkdir -p $(PREFIX_DIR)/bin
	mkdir -p $(PREFIX_DIR)/lib
	mkdir -p $(PREFIX_DIR)/include
	cp -v out/libacx1.so $(PREFIX_DIR)/lib/
	[ `whoami` != root ] || ldconfig
	-rm -rf $(PREFIX_DIR)/include/acx1
	cp -vr include $(PREFIX_DIR)/
	cp -v out/linesel $(PREFIX_DIR)/bin/
	
uninstall:
	-rm -f $(PREFIX_DIR)/lib/libacx1.so $(PREFIX_DIR)/bin/linesel
	-rm -rf $(PREFIX_DIR)/include/acx1.h $(PREFIX_DIR)/include/acx1
	[ `whoami` != root ] || ldconfig
	
out/libacx1.so: out/gnulinux.o out/common.o | out/
	gcc -shared -o$@ $^ -lpthread -lc41
	
out/gnulinux.o: src/gnulinux.c | out/
	gcc -c $(gcc_flags) -Iinclude -O3 -fpic -o$@ -DACX1_DLIB_BUILD -DNDEBUG $<

out/common.o: src/common.c | out/
	gcc -c $(gcc_flags) -Iinclude -O3 -fpic -o$@ -DACX1_DLIB_BUILD -DNDEBUG $<

out/test: src/test.c | out/
	gcc $(gcc_flags) -O3 -fPIC -o$@ -Iinclude -DACX1_DLIB_BUILD -DNDEBUG $< -Lout/ -lacx1

linesel: out/linesel dlib 
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):out $< < src/linesel.c

out/linesel: src/linesel.c | out/
	gcc $(gcc_flags) -O3 -fPIC -o$@ -Iinclude -DNDEBUG $< -Lout -lacx1 -lc41

