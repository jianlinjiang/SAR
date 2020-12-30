// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef SAR_UTIL_H
#define SAR_UTIL_H

#include "sgx_urts.h"
#include "enclave_u.h"
#include "sgx_ukey_exchange.h"
#include "sgx_uae_epid.h"
#include "sgx_uae_quote_ex.h"
#include <butil/logging.h>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <fstream>
#define TOKEN_FILENAME "enclave.token"
#define ENCLAVE_FILENAME "enclave.signed.so"

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

typedef struct aggregation_arguments_t {
  char a; //aggregation method
  int n; // total_number
  int f; // byzantine number
  int m; // how many selected by krum;
  float r;  // random select 0.1
  int k; // how many selected by sar
}aggregation_arguments;

extern sgx_enclave_id_t global_eid;
extern const size_t err_length;
extern uint32_t g_extended_epid_group_id;
extern sgx_att_key_id_t g_selected_key_id;
extern std::map<std::string, sgx_ra_context_t> g_client_context_map;
extern std::mutex g_context_map_mutex;
// uuid to directory
extern std::map<std::string, int> g_client_directory_map;
// filename to fd
extern std::map<std::string, int> g_client_file_map;  
extern std::atomic<int> client_index;
const char* get_error_message(sgx_status_t ret);
bool check_arr_is_zero(const uint8_t*, const uint32_t);
#endif
