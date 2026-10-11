/* Copyright (C) 2026 Qore Technologies, s.r.o. */
/* a library that uses an OpenSSL: the other one, or the real one */
#ifdef OTHER_SSL
int qore_ssl_check_other_ssl(void);
int qore_ssl_check_dep(void) { return qore_ssl_check_other_ssl(); }
#else
#include <openssl/ssl.h>
int qore_ssl_check_dep(void) { return OPENSSL_init_ssl(0, NULL); }
#endif
