#ifndef PPR_INTERFACE_H
#define PPR_INTERFACE_H

#if __cplusplus
extern "C" {
#endif

typedef unsigned char * ppr_uint256; // 32byte array for public and private keys
typedef unsigned char * ppr_uint512; // 64byte array for signatures
typedef void * ppr_transaction;

// Convert public/private key bytes 'source' to a 64 byte not-null-terminated hex string 'destination'
void ppr_uint256_to_string (const ppr_uint256 source, char * destination);
// Convert public key bytes 'source' to a 65 byte non-null-terminated account string 'destination'
void ppr_uint256_to_address (ppr_uint256 source, char * destination);
// Convert public/private key bytes 'source' to a 128 byte not-null-terminated hex string 'destination'
void ppr_uint512_to_string (const ppr_uint512 source, char * destination);

// Convert 64 byte hex string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int ppr_uint256_from_string (const char * source, ppr_uint256 destination);
// Convert 128 byte hex string 'source' to a byte array 'destination'
// Return 0 on success, nonzero on error
int ppr_uint512_from_string (const char * source, ppr_uint512 destination);

// Check if the null-terminated string 'account' is a valid ppr account number
// Return 0 on correct, nonzero on invalid
int ppr_valid_address (const char * account);

// Create a new random number in to 'destination'
void ppr_generate_random (ppr_uint256 destination);
// Retrieve the deterministic private key for 'seed' at 'index'
void ppr_seed_key (const ppr_uint256 seed, int index, ppr_uint256);
// Derive the public key 'pub' from 'key'
void ppr_key_account (ppr_uint256 key, ppr_uint256 pub);

// Sign 'transaction' using 'private_key' and write to 'signature'
char * ppr_sign_transaction (const char * transaction, const ppr_uint256 private_key);
// Generate work for 'transaction'
char * ppr_work_transaction (const char * transaction);

#if __cplusplus
} // extern "C"
#endif

#endif // PPR_INTERFACE_H
