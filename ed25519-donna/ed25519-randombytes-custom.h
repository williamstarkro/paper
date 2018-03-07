/*
	a custom randombytes must implement:

	void ED25519_FN(ed25519_randombytes_unsafe) (void *p, size_t len);

	ed25519_randombytes_unsafe is used by the batch verification function
	to create random scalars
*/

#include <cryptopp/osrng.h>

namespace rai
{
extern CryptoPP::AutoSeededRandomPool random_pool;
}

void ed25519_randombytes_unsafe (void * out, size_t outlen)
{
    rai::random_pool.GenerateBlock (reinterpret_cast <uint8_t *> (out), outlen);
}