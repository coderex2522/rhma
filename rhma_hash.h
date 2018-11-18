#ifndef RHMA_HASH_H
#define RHMA_HASH_H

typedef uint32_t (*rhma_hash_func)(const void *key, size_t length);
rhma_hash_func hash;

void rhma_hash_init();

#endif
