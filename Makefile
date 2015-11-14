DEBUG=yes
ENABLE_SERVER=yes

USE_SYSTEM_LINENOISE=no
USE_SYSTEM_LZ4=no
USE_SYSTEM_DIVSUFSORT=no
USE_SYSTEM_ART=no
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

VIO_SRCS=context.c dict.c emit.c error.c gc.c math.c opcodes.c repl.c serialize.c tok.c uneval.c val.c vm.c
SRCS=$(VIO_SRCS) flag.c
VIO_HEADERS=$(VIO_SRCS:.c=.h)
VIO_HEADERS:=$(filter-out repl.h,$(VIO_HEADERS))

ifeq ($(ENABLE_SERVER),yes)
	VIO_SRCS+=server.c
	VIO_HEADERS+=server.h
	LIBS+=-lmagic
	CFLAGS+=-DVIO_SERVER
endif

ifeq ($(USE_SYSTEM_LINENOISE),no)
	SRCS+=linenoise.c
else
	LIBS+=-llinenoise
	CFLAGS+=-DUSE_SYSTEM_LINENOISE
endif
ifeq ($(USE_SYSTEM_LZ4),no)
	SRCS+=lz4.c
else
	LIBS+=-llz4
	CFLAGS+=-DUSE_SYSTEM_LZ4
endif
ifeq ($(USE_SYSTEM_DIVSUFSORT),no)
	SRCS+=divsufsort.c sssort.c trsort.c utils.c
else
	LIBS+=-ldivsufsort
	CFLAGS+=-DUSE_SYSTEM_DIVSUFSORT
endif
ifeq ($(USE_SYSTEM_ART),no)
	SRCS+=art.c
else
	LIBS+=-lart
	CFLAGS+=-DUSE_SYSTEM_ART
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

all: $(SRCS) $(IVIO)

$(IVIO): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

.PHONY: doc

doc:
	cldoc generate $(INCLUDE_DIRS) -- --output doc --language c $(VIO_HEADERS)

.PHONY: clean

clean:
	rm $(OBJS) $(IVIO)
