DEBUG=yes
ENABLE_SERVER=yes
ENABLE_WEBREPL=yes

USE_SYSTEM_LINENOISE=no
USE_SYSTEM_LZF=no
USE_SYSTEM_DIVSUFSORT=no
USE_SYSTEM_SQLITE=no

BLAS_IMPL=openblas

CC=gcc5
CFLAGS+=-std=gnu11 -Wall -ftree-vectorize -msse4
CFLAGS+=-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION
LDFLAGS+=-Wl,-rpath=/usr/local/lib/gcc5
ifeq ($(DEBUG),yes)
	CFLAGS+=-g -ftree-vectorizer-verbose=2
else
	CFLAGS+=-O2
endif

INCLUDE_DIRS=-I/usr/include -I/usr/local/include

LIBS=-lm -lgmp -l$(BLAS_IMPL)

VIO_SRCS=bytecode.c context.c dict.c error.c gc.c math.c opcodes.c parsercombinators.c repl.c rewrite.c serialize.c tok.c uneval.c val.c vm.c
SRCS=$(VIO_SRCS) flag.c art.c mpc.c tpl.c
VIO_HEADERS=$(VIO_SRCS:.c=.h)
VIO_HEADERS:=$(filter-out repl.h,$(VIO_HEADERS))

ifeq ($(ENABLE_SERVER),yes)
	VIO_SRCS+=server.c
	VIO_HEADERS+=server.h
	LIBS+=-lmagic
	CFLAGS+=-DVIO_SERVER
endif

FETCH_DEPS=
ifeq ($(ENABLE_WEBREPL),yes)
ifeq ($(wildcard webrepl/jq-console/.),)
	FETCH_DEPS+=fetch-webrepl
endif
	CFLAGS+=-DVIO_WEBREPL
endif

ifeq ($(USE_SYSTEM_LINENOISE),no)
	SRCS+=linenoise.c
else
	LIBS+=-llinenoise
	CFLAGS+=-DUSE_SYSTEM_LINENOISE
endif
ifeq ($(USE_SYSTEM_LZF),no)
	SRCS+=lzf_c.c lzf_d.c
else
	LIBS+=-llzf
	CFLAGS+=-DUSE_SYSTEM_LZF
endif
ifeq ($(USE_SYSTEM_DIVSUFSORT),no)
	SRCS+=divsufsort.c sssort.c trsort.c utils.c
else
	LIBS+=-ldivsufsort
	CFLAGS+=-DUSE_SYSTEM_DIVSUFSORT
endif
ifeq ($(USE_SYSTEM_SQLITE),no)
	#Don't enable until I use it
	#SRCS+=sqlite3.c
else
	LIBS+=-lsqlite3
	CFLAGS+=-DUSE_SYSTEM_SQLITE
endif

OBJS=$(SRCS:.c=.o)
IVIO=vio

all: fetchdeps $(SRCS) $(IVIO)

$(IVIO): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

fetchdeps: $(FETCH_DEPS)

JQCONSOLE_UPSTREAM=https://github.com/replit/jq-console.git
.PHONY: fetch-webrepl
fetch-webrepl:
	if [ -d "webrepl/jq-console" ]; then \
		cd webrepl/jq-console && \
		git pull $(JQCONSOLE_UPSTREAM) && \
		cd ..; \
	else \
		git clone $(JQCONSOLE_UPSTREAM) webrepl/jq-console; \
	fi

.PHONY: doc
doc:
	cldoc generate $(INCLUDE_DIRS) -- --output doc --language c $(VIO_HEADERS)

.PHONY: clean
clean:
	rm $(OBJS) $(IVIO)

.PHONY: clean-deps
clean-deps:
	rm -rf webrepl/jq-console

.PHONY: semicolons
semicolons:
	fgrep -o ';' $(VIO_SRCS) $(VIO_HEADERS) | wc -l | tr -d '[:space:]' | xargs -0 printf '%d semicolons (including header files)'
