TOPDIR=../../..
include ../../../config.mk


all:
	(export PATH=$$PATH:$(VX32SDK)/bin; \
		vx32-gcc \
			-ggdb \
			-mfp-ret-in-387 \
			-I $(FOREIGN)/include \
			-I ../external/vx32/src/libvxc \
			smalltable.c \
			-c -o $(FOREIGN)/lib/smalltable.o )
	cp -u smalltable.h  $(FOREIGN)/include

blah:
	test-multiple-client test-multiple
	valgrind --leak-check=full --show-reachable=yes ./test-multiple test-multiple-client

test-multiple-client: test-multiple-client.c
	(export PATH=$$PATH:$(PREFIX)/bin; \
		GCC_EXEC_PREFIX=../vx32/src/libvxc/vx32/ vx32-gcc \
			-fpic\
			-ggdb \
			-mfp-ret-in-387 \
			-o $@ $^ \
			-I../vx32/src/libvxc/include \
			-L../vx32/src/libvxc \
			-Wl,-T$(PREFIX)/vx32/lib/ldscripts/elf_i386.x)


test-multiple: test-multiple.c
	gcc -Wall  $^ -I ../vx32/src/ ../vx32/src/libvx32/libvx32.a -o $@


clean:
	rm -f test-multiple-client test-multiple

