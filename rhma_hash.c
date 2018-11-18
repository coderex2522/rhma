#include "murmur3_hash.h"
#include "rhma_hash.h"


void rhma_hash_init()
{
	hash=MurmurHash3_x86_32;
}