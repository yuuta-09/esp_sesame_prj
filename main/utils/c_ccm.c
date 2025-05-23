

#include <string.h>
#include "aes-cbc-cmac.h"
#include "c_ccm.h"

#define CCM_ENCRYPT 0
#define CCM_DECRYPT 1

static int aes_ecb_encrypt(const uint8_t * pKey, uint8_t * input, uint8_t * output)
{
    AES_128_ENC(pKey, input, output);

    return 0;
}

/* Implementation that should never be optimized out by the compiler */
static void mbedtls_zeroize(void * v, size_t n)
{
    volatile unsigned char * p = v;
    while (n--)
        *p++ = 0;
}

/*
 * Macros for common operations.
 * Results in smaller compiled code than static inline functions.
 */

/*
 * Update the CBC-MAC state in y using a block in b
 * (Always using b as the source helps the compiler optimise a bit better.)
 */
#define UPDATE_CBC_MAC_1                                                                                                                                                                                                                                      \
    for (i = 0; i < 16; i++)                                                                                                                                                                                                                                  \
        y[i] ^= b[i];                                                                                                                                                                                                                                         \
                                                                                                                                                                                                                                                              \
    if ((ret = aes_ecb_encrypt(key, y, y)) != 0)                                                                                                                                                                                                              \
        return (ret);

/*
 * Encrypt or decrypt a partial block with CTR
 * Warning: using b for temporary storage! src and dst must not be b!
 * This avoids allocating one more 16 bytes buffer while allowing src == dst.
 */
#define CTR_CRYPT_1(dst, src, len)                                                                                                                                                                                                                            \
    if ((ret = aes_ecb_encrypt(key, ctr, b)) != 0)                                                                                                                                                                                                            \
        return (ret);                                                                                                                                                                                                                                         \
                                                                                                                                                                                                                                                              \
    for (i = 0; i < len; i++)                                                                                                                                                                                                                                 \
        dst[i] = src[i] ^ b[i];

/*
 * Authenticated encryption or decryption
 */
static int ccm_auth_crypt(int mode, const unsigned char * key, const unsigned char * iv, size_t iv_len, const unsigned char * add, size_t add_len, const unsigned char * input, size_t length, unsigned char * output, unsigned char * tag, size_t tag_len)
{
    int ret;
    unsigned char i;
    unsigned char q;
    size_t len_left;
    unsigned char b[16];
    unsigned char y[16];
    unsigned char ctr[16];
    const unsigned char * src;
    unsigned char * dst;

    /*
     * Check length requirements: SP800-38C A.1
     * Additional requirement: a < 2^16 - 2^8 to simplify the code.
     * 'length' checked later (when writing it to the first block)
     */
    if (tag_len < 4 || tag_len > 16 || tag_len % 2 != 0)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    /* Also implies q is within bounds */
    if (iv_len < 7 || iv_len > 13)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    if (add_len > 0xFF00)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    q = 16 - 1 - (unsigned char) iv_len;

    /*
     * First block B_0:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       length
     *
     * With flags as (bits):
     * 7        0
     * 6        add present?
     * 5 .. 3   (t - 2) / 2
     * 2 .. 0   q - 1
     */
    b[0] = 0;
    b[0] |= (add_len > 0) << 6;
    b[0] |= ((tag_len - 2) / 2) << 3;
    b[0] |= q - 1;

    memcpy(b + 1, iv, iv_len);

    for (i = 0, len_left = length; i < q; i++, len_left >>= 8)
        b[15 - i] = (unsigned char) (len_left & 0xFF);

    if (len_left > 0)
        return (MBEDTLS_ERR_CCM_BAD_INPUT);

    /* Start CBC-MAC with first block */
    memset(y, 0, 16);
    UPDATE_CBC_MAC_1;

    /*
     * If there is additional data, update CBC-MAC with
     * add_len, add, 0 (padding to a block boundary)
     */
    if (add_len > 0)
    {
        size_t use_len;
        len_left = add_len;
        src      = add;

        memset(b, 0, 16);
        b[0] = (unsigned char) ((add_len >> 8) & 0xFF);
        b[1] = (unsigned char) ((add_len) &0xFF);

        use_len = len_left < 16 - 2 ? len_left : 16 - 2;
        memcpy(b + 2, src, use_len);
        len_left -= use_len;
        src += use_len;

        UPDATE_CBC_MAC_1;

        while (len_left > 0)
        {
            use_len = len_left > 16 ? 16 : len_left;

            memset(b, 0, 16);
            memcpy(b, src, use_len);
            UPDATE_CBC_MAC_1;

            len_left -= use_len;
            src += use_len;
        }
    }

    /*
     * Prepare counter block for encryption:
     * 0        .. 0        flags
     * 1        .. iv_len   nonce (aka iv)
     * iv_len+1 .. 15       counter (initially 1)
     *
     * With flags as (bits):
     * 7 .. 3   0
     * 2 .. 0   q - 1
     */
    ctr[0] = q - 1;
    memcpy(ctr + 1, iv, iv_len);
    memset(ctr + 1 + iv_len, 0, q);
    ctr[15] = 1;

    /*
     * Authenticate and {en,de}crypt the message.
     *
     * The only difference between encryption and decryption is
     * the respective order of authentication and {en,de}cryption.
     */
    len_left = length;
    src      = input;
    dst      = output;

    while (len_left > 0)
    {
        size_t use_len = len_left > 16 ? 16 : len_left;

        if (mode == CCM_ENCRYPT)
        {
            memset(b, 0, 16);
            memcpy(b, src, use_len);
            UPDATE_CBC_MAC_1;
        }

        CTR_CRYPT_1(dst, src, use_len);

        if (mode == CCM_DECRYPT)
        {
            memset(b, 0, 16);
            memcpy(b, dst, use_len);
            UPDATE_CBC_MAC_1;
        }

        dst += use_len;
        src += use_len;
        len_left -= use_len;

        /*
         * Increment counter.
         * No need to check for overflow thanks to the length check above.
         */
        for (i = 0; i < q; i++)
            if (++ctr[15 - i] != 0)
                break;
    }

    /*
     * Authentication: reset counter and crypt/mask internal tag
     */
    for (i = 0; i < q; i++)
        ctr[15 - i] = 0;

    CTR_CRYPT_1(y, y, 16);
    memcpy(tag, y, tag_len);

    return (0);
}

/*
 * Authenticated encryption
 */
int aes_ccm_encrypt_and_tag(const unsigned char * key, const unsigned char * iv, size_t iv_len, const unsigned char * add, size_t add_len, const unsigned char * input, size_t length, unsigned char * output, unsigned char * tag, size_t tag_len)
{
    return (ccm_auth_crypt(CCM_ENCRYPT, key, iv, iv_len, add, add_len, input, length, output, tag, tag_len));
}

/*
 * Authenticated decryption
 */
int aes_ccm_auth_decrypt(const unsigned char * key, const unsigned char * iv, size_t iv_len, const unsigned char * add, size_t add_len, const unsigned char * input, size_t length, unsigned char * output, const unsigned char * tag, size_t tag_len)
{
    int ret;
    unsigned char check_tag[16];
    unsigned char i;
    int diff;

    if ((ret = ccm_auth_crypt(CCM_DECRYPT, key, iv, iv_len, add, add_len, input, length, output, check_tag, tag_len)) != 0)
    {
        return (ret);
    }

    /* Check tag in "constant-time" */
    for (diff = 0, i = 0; i < tag_len; i++)
        diff |= tag[i] ^ check_tag[i];

    if (diff != 0)
    {
        mbedtls_zeroize(output, length);
        return (MBEDTLS_ERR_CCM_AUTH_FAILED);
    }

    return (0);
}
