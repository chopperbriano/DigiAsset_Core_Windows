/*
** Stub header for OpenSSL BIO
** This minimal header allows compilation to proceed
*/

#ifndef OPENSSL_BIO_H
#define OPENSSL_BIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bio_st BIO;
typedef struct bio_method_st BIO_METHOD;

#define BIO_FLAGS_READ  0x01
#define BIO_FLAGS_WRITE 0x02
#define BIO_FLAGS_BASE64_NO_NL 0x100

BIO_METHOD* BIO_f_base64(void);
BIO* BIO_new(BIO_METHOD* method);
BIO* BIO_new_mem_buf(const void* buf, int len);
BIO* BIO_push(BIO* bio, BIO* append);
int BIO_set_flags(BIO* bio, int flags);
int BIO_read(BIO* bio, void* data, int len);
int BIO_free_all(BIO* bio);

#ifdef __cplusplus
}
#endif

#endif /* OPENSSL_BIO_H */
