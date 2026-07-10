#include "BleCrypto.h"
#include "mbedtls/md.h"
#include <cstring>

namespace ilss {

int hkdfSha256(const uint8_t* salt, size_t salt_len,
               const uint8_t* ikm, size_t ikm_len,
               const uint8_t* info, size_t info_len,
               uint8_t* okm, size_t okm_len) {
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md || okm_len == 0 || okm_len > 255 * 32) return -1;

    uint8_t prk[32];
    uint8_t zeros[32]{};
    const uint8_t* used_salt = (salt && salt_len) ? salt : zeros;
    size_t used_salt_len = (salt && salt_len) ? salt_len : sizeof(zeros);
    if (mbedtls_md_hmac(md, used_salt, used_salt_len, ikm, ikm_len, prk) != 0) {
        return -1;
    }

    uint8_t t[32]{};
    size_t t_len = 0;
    size_t offset = 0;
    uint8_t counter = 1;
    while (offset < okm_len) {
        mbedtls_md_context_t ctx;
        mbedtls_md_init(&ctx);
        if (mbedtls_md_setup(&ctx, md, 1) != 0) {
            mbedtls_md_free(&ctx);
            return -1;
        }
        if (mbedtls_md_hmac_starts(&ctx, prk, sizeof(prk)) != 0 ||
            (t_len && mbedtls_md_hmac_update(&ctx, t, t_len) != 0) ||
            (info_len && info && mbedtls_md_hmac_update(&ctx, info, info_len) != 0) ||
            mbedtls_md_hmac_update(&ctx, &counter, 1) != 0 ||
            mbedtls_md_hmac_finish(&ctx, t) != 0) {
            mbedtls_md_free(&ctx);
            return -1;
        }
        mbedtls_md_free(&ctx);
        t_len = 32;
        size_t copy = (okm_len - offset < 32) ? (okm_len - offset) : 32;
        std::memcpy(okm + offset, t, copy);
        offset += copy;
        ++counter;
    }
    return 0;
}

}  // namespace ilss
