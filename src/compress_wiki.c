#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "divsufsort.h"
#include "fse.h"
#include "lzf.h"

int main(int argc, char **argv) {
    FILE *f = fopen("webrepl/wiki.html", "r"), *o = fopen("webrepl/wiki.html.lzf", "wb");
    struct stat st;
    fstat(fileno(f), &st);
    uint32_t in_len = st.st_size, out_len;
    uint8_t *in = malloc(in_len), *out = malloc(in_len);
    saint_t idx;
    fread(in, 1, in_len, f);
    idx = divbwt(in, in, NULL, in_len);
    out_len = lzf_compress(in, in_len, out, in_len);
/* buggy */
#if 0
    in = out;
    in_len = out_len;
    /* FSE/HUFF0 uses opposite argument order to LZF */
    out_len = FSE_compress(out, st.st_size, in, in_len);
#endif
    fwrite(&idx, 1, sizeof(saint_t), o);
    fwrite(out, out_len, 1, o);
    fclose(f);
    fclose(o);
}
