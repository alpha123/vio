DEBUG=yes

USE_SYSTEM_LINENOISE=no
USE_SYSTEM_LZ4=no
USE_SYSTEM_DIVSUFSORT=no
USE_SYSTEM_ART=no

BLAS_IMPL=openblas

CFLAGS=-std=c99 -pedantic
ifeq ($(DEBUG),yes)
	CFLAGS+=-g
else
	CFLAGS+=-O2
endif

INCLUDE_DIRS=-I/usr/include -I/usr/local/include

LIBS=-lm -lgmp -l$(BLAS_IMPL)

SRCS=context.c dict.c gc.c math.c opcodes.c repl.c tok.c val.c vm.c

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

OBJS=$(SRCS:.c=.o)
IVIO=vio

all: $(SRCS) $(IVIO)

$(IVIO): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

.PHONY: clean

clean:
	rm $(OBJS) $(IVIO)
