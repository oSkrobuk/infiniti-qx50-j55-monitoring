#pragma once

#include <openssl/hmac.h>

using mbedtls_md_info_t = EVP_MD;
static constexpr int MBEDTLS_MD_SHA256 = 1;
inline const mbedtls_md_info_t *mbedtls_md_info_from_type(int) { return EVP_sha256(); }
inline int mbedtls_md_hmac(const mbedtls_md_info_t *info, const unsigned char *key, size_t key_size,
                           const unsigned char *data, size_t size, unsigned char *digest)
{
    unsigned int digest_size = 0;
    return HMAC(info, key, static_cast<int>(key_size), data, size, digest, &digest_size) == nullptr ? -1 : 0;
}
