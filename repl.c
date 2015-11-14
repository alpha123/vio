#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef USE_SYSTEM_LINENOISE
#include <linenoise.h>
#else
#include "linenoise.h"
#endif
#include "flag.h"
#include "context.h"
#include "emit.h"
#include "error.h"
#include "opcodes.h"
#include "serialize.h"
#include "server.h"
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

#define VERSION "This is Vio, version 0.0.0-0, built with " \
                COMPILER "." \
                "\nCopyright (c) 2015 Peter Cannici <turkchess123@gmail.com>"

char *do_expr(vio_ctx *ctx, const char *expr) {
    vio_err_t err = 0;
    vio_opcode *prog;
    vio_val **consts;
    vio_tok *t;
    uint32_t plen, clen;
    CHECK(vio_tokenize_str(&t, expr, strlen(expr)));
    CHECK(vio_emit(ctx, t, &plen, &prog, &clen, &consts));
    vio_exec(ctx, prog, consts);
    return vio_uneval(ctx);

    error:
    vio_tok_free_all(t);
    free(prog);
    free(consts);
    fprintf(stderr, "Error: %s", vio_err_msg(err));
    exit(1);
    return NULL;
}

int main(int argc, const char **argv) {
    const char *expr = NULL, *txt_file = NULL, *bc_file = NULL, *out_file = NULL;

#ifdef VIO_SERVER
    int server_port = 0;
    flag_int(&server_port, "S", "port\tRun as a server. Serves files in the current directory, executing any vio images.");
#endif

    flag_str(&expr, "e", "expr\tEvaluate an expression and exit.");
    flag_str(&txt_file, "c", "file\tCompile a file to bytecode. If -- is specified for the file, read from STDIN.");
    flag_str(&bc_file, "r", "file\tRun a compiled bytecode file.");
    flag_str(&out_file, "o", "file\tSpecify a file for the output of -c. By default bytecodes are written to STDOUT.");
    flag_parse(argc, argv, VERSION);

    vio_err_t err = 0;
    vio_ctx ctx;
    vio_open(&ctx);

    vio_opcode *prog;
    vio_val **consts;
    vio_tok *t, *tt;
    char *s, *line;
    uint32_t plen, clen;

#ifdef VIO_SERVER
    if (server_port) {
        printf("Listening on 127.0.0.1:%d\nCtrl-C to stop.\n", server_port);
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
        uint32_t bsz = 1024, z[] = {0, 0};
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
        CHECK(vio_emit(&ctx, st.fst, &plen, &prog, &clen, &consts));

        fwrite(&clen, sizeof(uint32_t), 1, out);
        for (uint32_t i = 0; i < clen; ++i)
            vio_dump_val(out, consts[i]);
        fwrite(z, sizeof(uint32_t), 2, out);
        fwrite(&plen, sizeof(uint32_t), 1, out);
        for (uint32_t i = 0; i < plen; ++i)
            fwrite(prog + i, sizeof(vio_opcode), 1, out);
        return 0;
    }
    else if (bc_file) {
    }

    puts(VERSION);
    puts("Use vio --help for usage information.");
    puts("Ctrl-C to quit.");
    putchar('\n');
    while ((line = linenoise("vio> "))) {
        if (line[0] == '\0')
            continue;

        if (strncmp(line, "lex ", 4) == 0) {
            CHECK(vio_tokenize_str(&t, line + 4, strlen(line) - 4));
            tt = t;
            printf("--- lexer output ---\n");
            while (tt) {
                printf("token#%s\t(%d)\t%.*s\n", vio_tok_type_name(tt->what), tt->len, tt->len, tt->s);
                tt = tt->next;
            }
            printf("--- end lexer output ---\n");
        }
        else if (strncmp(line, "disasm ", 7) == 0) {
            CHECK(vio_tokenize_str(&t, line + 7, strlen(line) - 7));
            CHECK(vio_emit(&ctx, t, &plen, &prog, &clen, &consts));
            printf("--- disassembly ---\n");
            for (uint32_t i = 0; i < clen; ++i) {
                s = vio_uneval_val(consts[i]);
                printf("%d\t.const\t%s\t%s\n", i, vio_val_type_name(consts[i]->what), s);
                free(s);
            }
            putchar('\n');
            for (uint32_t i = 0; i < plen; ++i) {
                printf("%d\t0x%020lx\t\t%s\t%d\t%d\t%d\t%d\n",
                    i, prog[i],
                    vio_instr_mneumonic(vio_opcode_instr(prog[i])),
                    vio_opcode_imm1(prog[i]), vio_opcode_imm2(prog[i]),
                    vio_opcode_imm3(prog[i]), vio_opcode_imm4(prog[i]));
            }
            printf("--- end disassembly ---\n");
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
        vio_tok_free_all(t);
        free(prog);
        free(consts);
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
