// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef SAR_UTIL_H
#define SAR_UTIL_H

#include "sgx_urts.h"
#include "enclave_u.h"
#include <butil/logging.h>

#define TOKEN_FILENAME "enclave.token"
#define ENCLAVE_FILENAME "enclave.signed.so"

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

extern sgx_enclave_id_t global_eid;
extern const size_t err_length;

const char* get_error_message(sgx_status_t ret);
bool check_arr_is_zero(const uint8_t*, const uint32_t);
#endif
