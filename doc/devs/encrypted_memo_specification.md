# Hive Encrypted Memo Specification

This document provides a comprehensive technical specification for Hive's encrypted memo format, based on the authoritative implementation in the hive repository.

## Overview

Encrypted memos allow users to send private messages within Hive transfer operations. The encryption uses ECDH (Elliptic Curve Diffie-Hellman) for key agreement and AES-256-CBC for symmetric encryption.

**Key characteristics:**
- Prefix: `#` character identifies encrypted memos
- Maximum size: 2048 bytes (total memo field, encrypted or not)
- Encoding: Base58
- Character set: UTF-8 (validated before encryption)

## Data Structure

### memo_content Structure

```cpp
struct memo_content
{
    public_key_type   from;       // 33 bytes - Sender's compressed public key
    public_key_type   to;         // 33 bytes - Recipient's compressed public key
    uint64_t          nonce;      // 8 bytes - Random/timestamp value
    uint32_t          check;      // 4 bytes - Integrity check field
    std::vector<char> encrypted;  // Variable - AES ciphertext
};
```

### Serialization Order

The `FC_REFLECT` macro determines serialization order:

```cpp
FC_REFLECT( hive::protocol::crypto_memo::memo_content, (from)(to)(nonce)(check)(encrypted) )
```

**Binary layout:**
```
[ from (33 bytes) | to (33 bytes) | nonce (8 bytes) | check (4 bytes) | encrypted (varint len + data) ]
```

### Field Specifications

| Field | Type | Size | Description |
|-------|------|------|-------------|
| `from` | `public_key_type` | 33 bytes | Compressed secp256k1 public key (sender). First byte is 0x02 or 0x03. |
| `to` | `public_key_type` | 33 bytes | Compressed secp256k1 public key (recipient). First byte is 0x02 or 0x03. |
| `nonce` | `uint64_t` | 8 bytes | Little-endian. Typically nanoseconds since epoch. |
| `check` | `uint32_t` | 4 bytes | Little-endian. First 32 bits of SHA256(encryption_key). |
| `encrypted` | `vector<char>` | Variable | Varint length prefix + AES-256-CBC ciphertext. |

## Encryption Algorithm

### Step 1: Validate Input

```cpp
FC_ASSERT( memo.size() > 0 && memo[0] == '#' );
```

The input memo must start with `#`. This marker triggers encryption.

### Step 2: Strip Marker

```cpp
plaintext = memo.substr(1);  // Remove '#' prefix
```

The `#` marker is removed before encryption. Only the message content is encrypted.

### Step 3: Generate Nonce

```cpp
nonce = nonce.value_or(fc::time_point::now().time_since_epoch().count());
```

- If not provided, defaults to current timestamp in **nanoseconds** since Unix epoch
- Using the same nonce for identical sender/recipient pairs produces identical ciphertext
- Nonce should be unique per message for security

### Step 4: ECDH Shared Secret Derivation

```cpp
shared_secret = sender_private_key.get_shared_secret(recipient_public_key);
```

**Implementation details (secp256k1):**

1. Parse recipient's compressed public key (33 bytes) into a secp256k1 point
2. Perform scalar multiplication: `shared_point = recipient_pubkey * sender_privkey`
3. Serialize the result as compressed public key (33 bytes)
4. Hash the point's coordinates (skip first format byte): `shared_secret = SHA512(point[1:33])`

```cpp
// From elliptic_secp256k1.cpp
secp256k1_ec_pubkey_tweak_mul(context, &pubkey, private_key_data);
secp256k1_ec_pubkey_serialize(context, pub.begin(), &pk_len, &pubkey, SECP256K1_EC_COMPRESSED);
return fc::sha512::hash(pub.begin() + 1, pub.size() - 1);  // Skip format byte
```

**Result:** 64-byte (512-bit) shared secret.

### Step 5: Encryption Key Derivation

```cpp
encryption_key = SHA512(nonce || shared_secret)
```

**Serialization:**
1. Pack nonce as little-endian uint64_t (8 bytes)
2. Pack shared_secret as raw bytes (64 bytes)
3. Compute SHA512 of the concatenation

```cpp
fc::sha512::encoder enc;
fc::raw::pack(enc, nonce);           // 8 bytes, little-endian
fc::raw::pack(enc, shared_secret);   // 64 bytes
encryption_key = enc.result();       // 64 bytes
```

**Result:** 64-byte (512-bit) encryption key.

### Step 6: Check Field Calculation

```cpp
check = SHA256(encryption_key)._hash[0]
```

- Compute SHA256 of the 64-byte encryption key
- Take the first 32 bits (4 bytes) of the hash as a little-endian uint32_t
- This provides integrity verification without exposing the encryption key

### Step 7: Plaintext Packing

The plaintext message is serialized with a varint length prefix:

```cpp
packed_plaintext = fc::raw::pack_to_vector(plaintext_string)
```

**Format:**
```
[ varint_length | message_bytes ]
```

- Length uses variable-length integer encoding (1-9 bytes depending on length)
- Message bytes are raw UTF-8

### Step 8: AES-256-CBC Encryption

```cpp
ciphertext = aes_encrypt(encryption_key, packed_plaintext)
```

**AES parameters:**
- **Algorithm:** AES-256-CBC (via OpenSSL EVP_aes_256_cbc)
- **Key:** First 32 bytes of encryption_key (bytes 0-31)
- **IV:** Bytes 32-47 of encryption_key (16 bytes)
- **Padding:** PKCS#7 (OpenSSL default)

```cpp
// From aes.cpp
EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                   (unsigned char*)&encryption_key,      // 256-bit key (bytes 0-31)
                   ((unsigned char*)&encryption_key)+32  // 128-bit IV (bytes 32-47)
);
```

**Output:** Ciphertext with PKCS#7 padding (length is multiple of 16 bytes).

### Step 9: Assemble memo_content

```cpp
memo_content result;
result.from = sender_public_key;
result.to = recipient_public_key;
result.nonce = nonce;
result.check = check;
result.encrypted = ciphertext;
```

### Step 10: Base58 Encoding

```cpp
base58_encoded = fc::to_base58(fc::raw::pack_to_vector(memo_content))
```

**Base58 alphabet (Bitcoin standard):**
```
123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz
```

Note: Excludes `0`, `O`, `I`, `l` to avoid visual confusion.

### Step 11: Add Prefix

```cpp
encrypted_memo = '#' + base58_encoded
```

**Final format:** `#<base58_encoded_memo_content>`

## Decryption Algorithm

### Step 1: Validate and Parse

```cpp
FC_ASSERT(encrypted_memo.size() > 0 && encrypted_memo[0] == '#');
memo_content = from_base58_and_unpack(encrypted_memo.substr(1));
```

1. Verify `#` prefix
2. Base58 decode the remainder
3. Unpack into `memo_content` structure
4. **Validation:** Re-serialize and compare to ensure round-trip integrity

### Step 2: Find Decryption Key

The decryptor may be either the sender or recipient. Try both:

```cpp
private_key = key_finder(memo_content.from);
if (!private_key) {
    private_key = key_finder(memo_content.to);
    if (!private_key)
        return encrypted_memo;  // Cannot decrypt - return original
    shared_secret = private_key.get_shared_secret(memo_content.from);
} else {
    shared_secret = private_key.get_shared_secret(memo_content.to);
}
```

### Step 3: Regenerate Encryption Key

```cpp
encryption_key = SHA512(memo_content.nonce || shared_secret)
```

### Step 4: Verify Check Field

```cpp
computed_check = SHA256(encryption_key)._hash[0];
if (computed_check != memo_content.check)
    return encrypted_memo;  // Integrity check failed
```

This verifies the correct key was derived (wrong key = wrong check value).

### Step 5: AES Decryption

```cpp
packed_plaintext = aes_decrypt(encryption_key, memo_content.encrypted)
```

Using same key/IV derivation as encryption.

### Step 6: Unpack Plaintext

```cpp
plaintext = fc::raw::unpack_from_vector<std::string>(packed_plaintext)
```

Removes the varint length prefix and returns the original message string.

**Note:** The decrypted output does NOT include the `#` marker. The marker is only used to identify encrypted memos in the transfer operation.

## Key Types and Formats

### Private Key (WIF Format)

Hive private keys use Wallet Import Format (WIF):

```
Base58Check(0x80 || 32-byte-private-key || compression-flag)
```

Example: `5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze`

### Public Key (Compressed secp256k1)

33-byte compressed public key with network prefix:

```
prefix || Base58(33-byte-compressed-pubkey || 4-byte-checksum)
```

Prefixes:
- `STM` - Mainnet

Example: `STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW`

### Memo Keys

Each Hive account has a designated **memo public key** stored on-chain:

```cpp
account.memo_key  // Public key for receiving encrypted memos
```

To encrypt a memo:
- Sender uses their private memo key
- Recipient's public memo key is fetched from the blockchain

## Limits and Constraints

| Constraint | Value | Source |
|------------|-------|--------|
| Maximum memo size | 2048 bytes | `HIVE_MAX_MEMO_SIZE` |
| Character encoding | UTF-8 | Validated in `transfer_operation::validate()` |
| Encryption marker | `#` | First character triggers encryption |
| Minimum nonce | 0 | No minimum, but should be unique |
| Public key size | 33 bytes | Compressed secp256k1 |

### Size Calculation

Encrypted memo overhead (fixed):
- From public key: 33 bytes
- To public key: 33 bytes
- Nonce: 8 bytes
- Check: 4 bytes
- Total fixed: 78 bytes

Variable overhead:
- Varint length prefix: 1-3 bytes (for typical messages)
- AES padding: 1-16 bytes (PKCS#7)
- Base58 expansion: ~37% increase

## Worked Example

### Test Keys

From `beekeeper_tests.cpp`:
```cpp
// Key pair 0
Private: 5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze
Public:  STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW

// Key pair 1
Private: 5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su
Public:  STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME
```

### Encryption Flow (Pseudocode)

```python
# Input
input_memo = "#avocado-banana-cherry-durian"
from_privkey = decode_wif("5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze")
to_pubkey = decode_pubkey("STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME")
nonce = 777  # Fixed for reproducibility

# Step 1-2: Validate and strip marker
assert input_memo[0] == '#'
plaintext = "avocado-banana-cherry-durian"  # Without '#'

# Step 3: Nonce (provided)
# nonce = 777

# Step 4: ECDH
shared_secret = ecdh_sha512(from_privkey, to_pubkey)  # 64 bytes

# Step 5: Key derivation
encryption_key = sha512(pack_le_u64(777) + shared_secret)  # 64 bytes

# Step 6: Check field
check = unpack_le_u32(sha256(encryption_key)[0:4])

# Step 7: Pack plaintext
packed = varint_encode(len(plaintext)) + plaintext.encode('utf-8')

# Step 8: AES encrypt
key = encryption_key[0:32]   # 256-bit AES key
iv = encryption_key[32:48]   # 128-bit IV
ciphertext = aes_256_cbc_encrypt(packed, key, iv)

# Step 9: Assemble memo_content
memo_content = {
    'from': from_privkey.get_public_key(),  # 33 bytes
    'to': to_pubkey,                         # 33 bytes
    'nonce': 777,                            # 8 bytes
    'check': check,                          # 4 bytes
    'encrypted': ciphertext                  # variable
}

# Step 10-11: Encode and prefix
binary = serialize(memo_content)  # Order: from, to, nonce, check, encrypted
result = '#' + base58_encode(binary)
```

### Verification

With fixed nonce, same inputs produce same output:

```cpp
// From test
auto enc_777_a = encrypt_with_nonce(0, 1, wallet, "avocado-banana-cherry-durian", 777);
auto enc_777_b = encrypt_with_nonce(0, 1, wallet, "avocado-banana-cherry-durian", 777);
BOOST_REQUIRE(enc_777_a == enc_777_b);  // Same nonce = same output

auto enc_auto_a = encrypt_with_nonce(0, 1, wallet, content, std::nullopt);
auto enc_auto_b = encrypt_with_nonce(0, 1, wallet, content, std::nullopt);
BOOST_REQUIRE(enc_auto_a != enc_auto_b);  // Different nonces (auto-generated timestamps)
```

### Binary Format Verification

The encrypted memo starts with the `from` public key. Compressed secp256k1 public keys always begin with `0x02` or `0x03`:

```cpp
auto decoded = fc::from_base58(encrypted.substr(1));  // Remove '#'
uint8_t first_byte = static_cast<uint8_t>(decoded[0]);
BOOST_CHECK(first_byte == 0x02 || first_byte == 0x03);  // Compressed pubkey marker
```

## Error Handling

### Decryption Failures

The implementation returns the original encrypted memo string on failure (graceful degradation):

```cpp
std::string crypto_memo::decrypt(key_finder_type key_finder, const std::string& encrypted_memo)
{
    auto _c = load_from_string(encrypted_memo);
    if (!_c)
        return encrypted_memo;  // Parse failure

    auto _result = decrypt_impl(key_finder, _c->from, _c->to, _c.value());
    if (!_result.has_value())
        return encrypted_memo;  // Decryption failure

    return _result.value();
}
```

### Common Error Conditions

| Condition | Behavior |
|-----------|----------|
| No `#` prefix | Treated as plaintext, not decrypted |
| Invalid Base58 | Returns original string |
| Check field mismatch | Returns original string |
| No matching private key | Returns original string |
| AES decryption error | Returns original string |
| Unpack error | Returns original string |

## Security Considerations

### Nonce Uniqueness

Using the same nonce between the same sender/recipient produces identical ciphertext. Default behavior uses nanosecond timestamps to ensure uniqueness:

```cpp
nonce = fc::time_point::now().time_since_epoch().count();
```

### Forward Secrecy

This scheme does **not** provide forward secrecy. If a private key is compromised, all past messages encrypted with it can be decrypted.

### Key Reuse

The shared secret depends only on the key pair. The nonce provides per-message uniqueness, but the underlying ECDH secret is static for any given sender-recipient pair.

### Authenticity

The check field provides integrity verification but not authentication. An attacker who knows the encryption key can forge messages. The public keys in the structure indicate claimed sender/recipient but are not cryptographically bound to the ciphertext.

## Reference Implementation

### Source Files (hive repository)

| File | Description |
|------|-------------|
| `libraries/protocol/crypto_memo.cpp` | High-level memo encryption/decryption |
| `libraries/protocol/include/hive/protocol/crypto_memo.hpp` | Memo structure definition |
| `libraries/fc/src/crypto/crypto_data.cpp` | Core encryption implementation |
| `libraries/fc/include/fc/crypto/crypto_data.hpp` | Base crypto data structure |
| `libraries/fc/src/crypto/aes.cpp` | AES-256-CBC implementation |
| `libraries/fc/src/crypto/elliptic_secp256k1.cpp` | ECDH implementation |
| `libraries/fc/src/crypto/base58.cpp` | Base58 encoding/decoding |
| `libraries/wallet/wallet.cpp` | Wallet integration |

### Compatible Implementations

- **hive** (C++): Reference implementation
- **wax** (TypeScript/Python): Hive API library with memo support
- **hive-js** (@hiveio/hive-js): JavaScript library

---

*This specification is based on hive repository source code. For implementation details, consult the source files listed above.*
