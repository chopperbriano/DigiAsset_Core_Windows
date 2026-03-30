/*
** Stub header for OpenSSL EVP
** This minimal header allows compilation to proceed
*/

#ifndef OPENSSL_EVP_H
#define OPENSSL_EVP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct evp_md EVP_MD;
typedef struct evp_md_ctx EVP_MD_CTX;

EVP_MD_CTX *EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX *ctx);
const EVP_MD *EVP_sha256(void);
int EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type, void *impl);
int EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *d, size_t cnt);
int EVP_DigestFinal_ex(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *s);

#ifdef __cplusplus
}
#endif

#endif /* OPENSSL_EVP_H */
