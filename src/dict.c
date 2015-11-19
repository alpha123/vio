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
    void *idx = art_search(&dict->words, (const unsigned char *)key, klen);
    if (idx)
        *out = (uint32_t)idx - 1;
    return idx != NULL;
}

void vio_dict_store(vio_dict *dict, const char *key, uint32_t klen, uint32_t val) {
    /* art_search() returns NULL if a value is not found.
       Since 0 is a valid index to store, add 1 to it.
       Note that this means we can actually only store 2^32-1 words. */
    art_insert(&dict->words, (const unsigned char *)key, klen, (void *)(val + 1));
}
