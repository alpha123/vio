#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef USE_SYSTEM_LINENOISE
#include <linenoise.h>
#else
#include "linenoise.h"
#endif
#include "flag.h"
#include "bytecode.h"
#include "context.h"
#include "error.h"
#include "eval.h"
#include "opcodes.h"
#include "rewrite.h"
#include "serialize.h"
#include "server.h"
#include "stdvio.h"
#include "tok.h"
#include "uneval.h"
#include "val.h"
#include "vm.h"

#ifdef BSD
#include <sysexits.h>
#else
#define EX_CANTCREAT 73
#define EX_IOERR 74
#endif

#define CHECK(expr) if ((err = (expr))) goto error;

#ifdef __clang__
#define COMPILER "Clang " __VERSION__
#elif __GNUC__
#define COMPILER "GCC " __VERSION__
#else
#define COMPILER "unknown"
#endif

#define VIO_VERSION "0.0.2-0"

#define VERSION_INFO "This is Vio, version " VIO_VERSION " built with " \
                COMPILER "." \
                "\nCopyright (c) 2015 Peter Cannici <turkchess123@gmail.com>"

char *do_expr(vio_ctx *ctx, const char *expr) {
    vio_err_t err = 0;
    char *s;
    VIO__CHECK(vio_eval(ctx, -1, expr));
    s = vio_uneval(ctx);
    return s;

    error:
    printf("Error: %s\n", vio_err_msg(err));
    puts(ctx->err_msg);
    return NULL;
}

int main(int argc, const char **argv) {
    const char *expr = NULL, *txt_file = NULL, *bc_file = NULL, *out_file = NULL;

#ifdef VIO_SERVER
    int server_port = 0;
    flag_int(&server_port, "S", "port\tRun as a server. Serves files in the current directory. If a Vio image is accessed, it will launch an in-browser REPL web application.");
#endif

    flag_str(&expr, "e", "expr\tEvaluate an expression and exit.");
    flag_str(&txt_file, "c", "file\tCompile a text file to a vio image. If -- is specified for the file, read from STDIN.");
    flag_str(&bc_file, "i", "file\tLoad a vio image.");
    flag_str(&out_file, "o", "file\tSpecify a file for the output of -c. By default the image is written to STDOUT.");
    flag_parse(argc, argv, VERSION_INFO);

    vio_err_t err = 0;
    vio_ctx ctx;
    vio_open(&ctx);
    vio_load_stdlib(&ctx);

    vio_bytecode *bc;
    vio_tok *t, *tt;
    char *s, *line;
    uint32_t plen, clen;

#ifdef VIO_SERVER
    if (server_port) {
        printf("Listening on 127.0.0.1:%d\nCtrl-C Ctrl-C to stop.\n", server_port);
        vio_server_start(server_port);
    }
#endif

    FILE *in, *out = stdout;
    if (out_file) {
        out = fopen(out_file, "wb");
        if (out == NULL) {
            fprintf(stderr, "Could not open file %s for writing.", out_file);
            return EX_CANTCREAT;
        }
    }

    if (expr) {
        if ((s = do_expr(&ctx, expr))) {
            puts(s);
            free(s);
        }
        return 0;
    }
    else if (txt_file) {
        if (strcmp(txt_file, "--") == 0)
            in = stdin;
        else {
            in = fopen(txt_file, "r");
            if (in == NULL) {
                fprintf(stderr, "Could not open file %s for reading.", out_file);
                return EX_IOERR;
            }
        }
        vio_tokenizer st;
        vio_tokenizer_init(&st);
        uint32_t bsz = 1024;
        st.s = (char *)malloc(bsz);
        plen = 0;
        while (!feof(in)) {
            clen = fread((char *)st.s + plen, 1, 1024, in);
            if (clen == 1024) {
                bsz += 1024;
                st.s = realloc((char *)st.s, bsz);
            }
            plen += clen;
        }
        st.len = plen;
        CHECK(vio_tokenize(&st));
        CHECK(vio_emit(&ctx, st.fst, &bc));
        return vio_dump_bytecode(bc, out);
    }
    else if (bc_file) {
    }

    puts(VERSION_INFO);
    puts("Use vio -help for usage information.");
    puts("Ctrl-C to quit.");
    putchar('\n');
    while ((line = linenoise("vio> "))) {
        if (line[0] == '\0')
            continue;

        if (strncmp(line, "lex ", 4) == 0) {
            CHECK(vio_tokenize_str(&t, line + 4, strlen(line) - 4));
            CHECK(vio_rewrite(&t));
            tt = t;
            printf("--- lexer output ---\n");
            while (tt) {
                printf("token#%s\t(%d)\t%.*s\n", vio_tok_type_name(tt->what), tt->len, tt->len, tt->s);
                tt = tt->next;
            }
            printf("--- end lexer output ---\n");
            vio_tok_free_all(t);
        }
        else if (strncmp(line, "disasm ", 7) == 0) {
            if (strchr(line + 7, ' ')) {
                CHECK(vio_tokenize_str(&t, line + 7, strlen(line) - 7));
                CHECK(vio_rewrite(&t));
                CHECK(vio_emit(&ctx, t, &bc));
            }
            else {
                uint32_t bci;
                if (vio_dict_lookup(ctx.dict, line + 7, strlen(line) - 7, &bci))
                    bc = ctx.defs[bci];
                else
                    puts("unknown word");
            }
            printf("--- disassembly ---\n");
            for (uint32_t i = 0; i < bc->ic; ++i) {
                s = vio_uneval_val(bc->consts[i]);
                printf("%d\t.const\t%s\t%s\n", i, vio_val_type_name(bc->consts[i]->what), s);
                free(s);
            }
            putchar('\n');
            for (uint32_t i = 0; i < bc->ip; ++i) {
                printf("%d\t0x%020lx\t\t%s\t%d\t%d\t%d\t%d\n",
                    i, bc->prog[i],
                    vio_instr_mneumonic(vio_opcode_instr(bc->prog[i])),
                    vio_opcode_imm1(bc->prog[i]), vio_opcode_imm2(bc->prog[i]),
                    vio_opcode_imm3(bc->prog[i]), vio_opcode_imm4(bc->prog[i]));
            }
            printf("--- end disassembly ---\n");
            vio_tok_free_all(t);
            vio_bytecode_free(bc);
        }
        else if (strcmp(line, "ds") == 0 || strcmp(line, "dump-stack") == 0)
            vio_print_stack(&ctx, stdout);
        else if (strncmp(line, "save ", 5) == 0) {
            FILE *fp = fopen(line + 5, "wb");
            vio_dump(&ctx, fp);
            fclose(fp);
        }
        else if (strncmp(line, "load ", 5) == 0) {
            FILE *fp = fopen(line + 5, "rb");
            vio_load(&ctx, fp);
            fclose(fp);
        }
        else {
            s = do_expr(&ctx, line);
            if (s == NULL)
                printf("empty stack\n");
            else {
                puts(s);
                free(s);
            }
        }

        linenoiseHistoryAdd(line);
        free(line);
        t = NULL;
    }
    if (out != stdout)
        fclose(out);
    return 0;

    error:
    if (out != stdout)
        fclose(out);
    printf("error: %s", vio_err_msg(err));
    return err;
}
