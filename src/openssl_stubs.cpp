#include "openssl/bio.h"
#include <cstring>
#include <cstdlib>

struct BioHandle {
    const void* buf;
    int len;
    int pos;
    int flags;
};

BIO_METHOD* BIO_f_base64(void) {
    return nullptr; // stub
}

BIO* BIO_new(BIO_METHOD* method) {
    (void)method;
    return reinterpret_cast<BIO*>(new BioHandle());
}

BIO* BIO_new_mem_buf(const void* buf, int len) {
    BioHandle* h = new BioHandle();
    h->buf = buf;
    h->len = len;
    h->pos = 0;
    h->flags = 0;
    return reinterpret_cast<BIO*>(h);
}

BIO* BIO_push(BIO* bio, BIO* append) {
    (void)bio;
    (void)append;
    return bio; // stub
}

int BIO_set_flags(BIO* bio, int flags) {
    if (bio) {
        reinterpret_cast<BioHandle*>(bio)->flags = flags;
    }
    return 1;
}

int BIO_read(BIO* bio, void* data, int len) {
    if (!bio) return 0;
    BioHandle* h = reinterpret_cast<BioHandle*>(bio);
    int remaining = h->len - h->pos;
    int to_read = (len < remaining) ? len : remaining;
    if (to_read > 0) {
        memcpy(data, static_cast<const char*>(h->buf) + h->pos, to_read);
        h->pos += to_read;
    }
    return to_read;
}

int BIO_free_all(BIO* bio) {
    if (bio) {
        delete reinterpret_cast<BioHandle*>(bio);
    }
    return 1;
}