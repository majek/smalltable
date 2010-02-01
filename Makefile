CC = gcc
LD = $(CC)

DEBUG_CFLAGS = -g -fstack-protector-all -Wstack-protector -fstack-check

CPU = generic
ARCH = generic

#### CPU dependant optimizations
CPU_CFLAGS.generic    = -O2
CPU_CFLAGS.i686       = -O2 -march=i686
CPU_CFLAGS            = $(CPU_CFLAGS.$(CPU))

#### ARCH dependant flags, may be overriden by CPU flags
ARCH_FLAGS.generic    =
ARCH_FLAGS.i686       = -m32 -march=i686
ARCH_FLAGS.x86_64     = -m64 -march=x86-64
ARCH_FLAGS            = $(ARCH_FLAGS.$(ARCH))

#### Common CFLAGS
CFLAGS = $(ARCH_FLAGS) $(CPU_CFLAGS) $(DEBUG_CFLAGS)

#### Additional include and library dirs
ADDINC = 
ADDLIB = 

INC=-Ivx32/src
LIB=-levent

COPTS_COV=--coverage -DCOVERAGE_TEST 
COPTS_PROF=-pg

LDFLAGS_COV=--coverage
LDFLAGS_PROF=-pg

#### Options
include_path = $(patsubst %,-I%,$(1))
library_path = $(patsubst %,-L%,$(1))

OPTIONS_CFLAGS  =
OPTIONS_LDFLAGS =
OPTIONS_OBJS    =

ifneq ($(USE_TOKYOCABINET),)
TOKYOCABINETDIR :=
OPTIONS_CFLAGS  += -DCONFIG_USE_TOKYOCABINET $(call include_path,$(TOKYOCABINETDIR))
OPTIONS_LDFLAGS += $(call library_path,$(TOKYOCABINETDIR)) -ltokyocabinet
OPTIONS_OBJS    += src/sto_tc.o
endif

ifneq ($(USE_BERKELEYDB),)
BERKELEYDBDIR   :=
OPTIONS_CFLAGS  += -DCONFIG_USE_BERKELEYDB $(call include_path,$(BERKELEYDBDIR))
OPTIONS_LDFLAGS += $(call library_path,$(BERKELEYDBDIR)) -ldb
OPTIONS_OBJS    += src/sto_bdb.o
endif

ifneq ($(USE_YDB),)
YDBDIR          :=
OPTIONS_CFLAGS  += -DCONFIG_USE_YDB $(call include_path,$(YDBDIR))
OPTIONS_LDFLAGS += $(YDBDIR)/libydb.a
OPTIONS_OBJS    += src/sto_ydb.o
endif

COPTS  = -Wall
COPTS += $(CFLAGS) $(TARGET_CFLAGS)
COPTS += $(DEBUG) $(OPTIONS_CFLAGS) $(ADDINC) $(INC)

LDOPTS = $(TARGET_LDFLAGS) $(OPTIONS_LDFLAGS) $(ADDLIB) $(LIB)

OBJS = 	src/buffer.o         src/command.o  src/connection.o   src/framing.o \
	src/network.o        src/storage.o  src/sys_commands.o src/process.o \
	src/code_commands.o  src/common.o   src/event_loop.o   src/main.o \
	src/sto_fs.o         src/sto_dumb.o
	
PROXY_OBJS = src/buffer.o   src/connection.o   src/framing.o src/network.o \
	src/event_loop.o    src/proxy.o        src/common.o  src/rbtree.o \
	src/proxy_config.o  src/st_server.o    src/st_proxy.o \
	src/proxy_command.o src/proxy_sys_command.o src/proxy_client.o	\
	src/uevent.o

CLEAN_FILES=$(OBJS) $(PROXY_OBJS) src/*gcov src/cov/*.gcno src/cov/*.gcda tests/*.pyc


main:	prerequisits
	@echo 
	@echo "Please select storage backends you need and type "
	@echo 
	@echo "   $$ make all"
	@echo 
	@echo "Possible storage engines:"
	@echo "      USE_TOKYOCABINET - TokyoCabinet library"
	@echo "      USE_BERKELEYDB   - BerkeleyDB library"
	@echo "      USE_YDB          - YDB library"
	@echo
	@echo "Additional parameters:"
	@echo "      TOKYOCABINETDIR  - TokyoCabinet include and library dir"
	@echo "      BERKELEYDBDIR    - BerkeleyDB include and library dir"
	@echo "      YDBDIR           - YDB include and library dir"
	@echo
	@echo "Full featured build for me looks like:"
	@echo
	@echo "   $$ make all USE_TOKYOCABINET=y USE_BERKELEYDB=y USE_YDB=y YDBDIR=~/ydb/src"
	@echo
	@exit 1

DEBS=libevent-dev gcc make python
prerequisits:
	@if [ -e $(shell which dpkg) ]; then	\
		dpkg -l $(DEBS) >/dev/null || (	\
			echo;	\
			echo "Make sure you have all dependencies installed:";	\
			echo "    apt-get install $(DEBS)";		\
			echo;	\
			exit 1;);	\
	fi

all: smalltable smalltable-proxy server_tests

include Makevx32
include Maketests


smalltable:: $(OBJS) $(OPTIONS_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDOPTS)

smalltable-cov:: $(patsubst src/%, src/cov/%, $(OBJS) $(OPTIONS_OBJS))
	$(LD) $(LDFLAGS) $(LDFLAGS_COV) -o $@ $^ $(LDOPTS)

smalltable-prof:: $(patsubst src/%, src/prof/%, $(OBJS) $(OPTIONS_OBJS))
	$(LD) $(LDFLAGS) $(LDFLAGS_PROF) -o $@ $^ $(LDOPTS)

src/%.o:	src/%.c
	$(CC) $(COPTS) -c -o $@ $<

src/cov/%.o:	src/%.c
	$(CC) $(COPTS) $(COPTS_COV) -c -o $@ $<

src/prof/%.o:	src/%.c
	$(CC) $(COPTS) $(COPTS_PROF) -c -o $@ $<


CLEAN_FILES+=src/deps.d

%.o: src/deps.d
%.o: src/deps.d

src/deps.d:	$(wildcard src/*c)
	echo > $@
	@$(CC) $(COPTS) -MM $^ | sed 's/^\([^:]*\):/src\/\1:/' >> $@
	@$(CC) $(COPTS) -MM $^ | sed 's/^\([^:]*\):/src\/cov\/\1:/' >> $@
	@$(CC) $(COPTS) -MM $^ | sed 's/^\([^:]*\):/src\/proc\/\1:/' >> $@

-include src/deps.d


clean::
	rm -f smalltable smalltable-cov smalltable-prof
	rm -rf src/*.o src/cov/*.o src/prof/*.o
	rm -f $(CLEAN_FILES)

smalltable-proxy: $(PROXY_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDOPTS)

smalltable-proxy-cov:: $(patsubst src/%, src/cov/%, $(PROXY_OBJS))
	$(LD) $(LDFLAGS) $(LDFLAGS_COV) -o $@ $^ $(LDOPTS)

smalltable-proxy-prof:: $(patsubst src/%, src/prof/%, $(PROXY_OBJS))
	$(LD) $(LDFLAGS) $(LDFLAGS_PROF) -o $@ $^ $(LDOPTS)
