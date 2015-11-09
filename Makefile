DEBUG=yes

CFLAGS=-std=c99 -pedantic
ifeq ($(DEBUG),yes)
	CFLAGS+=-g
else
	CFLAGS+=-O2
endif

INCLUDE_DIRS=-I/usr/include -I/usr/local/include

LIBS=-lgmp -lopenblas

SRCS=context.c gc.c linenoise.c repl.c tok.c val.c
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
