DEBUG=yes
ENABLE_SERVER=yes

USE_SYSTEM_LINENOISE=no
USE_SYSTEM_LZF=no
USE_SYSTEM_DIVSUFSORT=no
USE_SYSTEM_SQLITE=no

BLAS_IMPL=openblas

GCCFLAGS=
ifneq ($(findstring "gcc",$(CC)),)
	GCCFLAGS+=-ftree-vectorize
ifeq ($(DEBUG),yes)
	GCCFLAGS+=-ftree-vectorizer-verbose=2
endif
endif

CFLAGS+=-std=gnu11 -Wall -msse4
CFLAGS+=-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION
ifeq ($(DEBUG),yes)
	CFLAGS+=-g -DDEBUG
else
	CFLAGS+=-O2 -DNDEBUG
endif

INCLUDE_DIRS=-I/usr/include -I/usr/local/include

LIBS=-lm -lgmp -l$(BLAS_IMPL)

VIO_SRCS=bytecode.c context.c dict.c error.c eval.c gc.c math.c \
opcodes.c parsercombinators.c repl.c rewrite.c serialize.c stdrules.c \
stdvio.c strings.c tok.c uneval.c val.c vm.c
SRCS=$(VIO_SRCS) art.c flag.c json.c mpc.c tpl.c civetweb.c
VIO_HEADERS=$(filter-out repl.h,$(VIO_SRCS:.c=.h))
WEBREPL_SRCS=webrepl/index.html webrepl/main.js

ifeq ($(ENABLE_SERVER),yes)
	VIO_SRCS+=server.c webrepl.c
	LIBS+=-lpthread
	CFLAGS+=-DVIO_SERVER -DUSE_WEBSOCKET -DNO_SSL
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

all: $(SRCS) $(IVIO)

$(IVIO): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(GCCFLAGS) $(INCLUDE_DIRS) -c $< -o $@

webrepl: $(WEBREPL_SRCS)
	cd webrepl && \
	npm install && browserify -t [ babelify --presets [ react es2015 ] ] main.js -o bundle.js && \
	cd .. && \
	xxd -i webrepl/index.html webrepl_html.i && \
	xxd -i webrepl/bundle.js webrepl_js.i

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
