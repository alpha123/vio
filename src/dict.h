#ifndef VIO_DICT_H
#define VIO_DICT_H

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include "art.h"
#include "types.h"
#include "error.h"

/* Map a word name to an integer index in an
   array of bytecode objects. */

typedef struct _vdict {
    art_tree words;
} vio_dict;

vio_err_t vio_dict_open(vio_dict *dict);
void vio_dict_close(vio_dict *dict);

/* returns non-zero iff the word exists in the dictionary */
int vio_dict_lookup(vio_dict *dict, const char *key, uint32_t klen, uint32_t *out);
void vio_dict_store(vio_dict *dict, const char *key, uint32_t klen, uint32_t val);

vio_function_info *vio_cdict_lookup(art_tree *cdict, const char *key, uint32_t klen);

#endif
