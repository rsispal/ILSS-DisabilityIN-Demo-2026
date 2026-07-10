#pragma once

#include <cstddef>
#include <cstdint>

namespace ilss {

/** HKDF-SHA256 via mbedtls HMAC (avoids CONFIG_MBEDTLS_HKDF_C). */
int hkdfSha256(const uint8_t* salt, size_t salt_len,
               const uint8_t* ikm, size_t ikm_len,
               const uint8_t* info, size_t info_len,
               uint8_t* okm, size_t okm_len);

}  // namespace ilss
