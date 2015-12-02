#include "alloca.h"
#include "dict.h"

vio_err_t vio_dict_open(vio_dict *dict) {
    if (art_tree_init(&dict->words))
        return VE_DICTIONARY_ALLOC_FAIL;
    return 0;
}

void vio_dict_close(vio_dict *dict) {
    art_tree_destroy(&dict->words);
}

int vio_dict_lookup(vio_dict *dict, const char *key, uint32_t klen, uint32_t *out) {
    /* art_search seems that it may have a bug/unexpected behavior where
       a key with non-null characters past klen doesn't find the value. */
    unsigned char *real_key = alloca(klen + 1);
    for (uint32_t i = 0; i < klen; ++i)
        real_key[i] = (unsigned char)key[i];
    real_key[klen] = '\0';
    void *idx = art_search(&dict->words, real_key, klen);
    if (idx)
        *out = (uint32_t)idx - 1;
    return idx != NULL;
}

vio_function_info *vio_cdict_lookup(art_tree *cdict, const char *key, uint32_t klen) {
    unsigned char *real_key = alloca(klen + 1);
    for (uint32_t i = 0; i < klen; ++i)
        real_key[i] = (unsigned char)key[i];
    real_key[klen] = '\0';
    return (vio_function_info *)art_search(cdict, real_key, klen);
}

void vio_dict_store(vio_dict *dict, const char *key, uint32_t klen, uint32_t val) {
    /* art_search() returns NULL if a value is not found.
       Since 0 is a valid index to store, add 1 to it.
       Note that this means we can actually only store 2^32-1 words. */
    art_insert(&dict->words, (const unsigned char *)key, klen, (void *)(val + 1));
}
