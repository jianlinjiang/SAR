// MIT License

// Copyright (c) 2020 jianlinjiang
#include <atomic>
#include "sgx_urts.h"
#include "enclave_u.h"
#include "sar_util.h"

sgx_enclave_id_t global_eid = 0;
uint32_t g_extended_epid_group_id = 0;
sgx_att_key_id_t g_selected_key_id = { 0 };
std::map<std::string, sgx_ra_context_t> g_client_context_map;
std::mutex g_context_map_mutex;
// use the index as the directory name
std::map<std::string, int> g_client_directory_map;  
std::atomic<int> client_index(0);
std::map<std::string, int> g_client_file_map;
sgx_errlist_t sgx_errlist[] = {
    {SGX_ERROR_UNEXPECTED,
     "Unexpected error occurred.",
     NULL},
    {SGX_ERROR_INVALID_PARAMETER,
     "Invalid parameter.",
     NULL},
    {SGX_ERROR_OUT_OF_MEMORY,
     "Out of memory.",
     NULL},
    {SGX_ERROR_ENCLAVE_LOST,
     "Power transition occurred.",
     "Please refer to the sample \"PowerTransition\" for details."},
    {SGX_ERROR_INVALID_ENCLAVE,
     "Invalid enclave image.",
     NULL},
    {SGX_ERROR_INVALID_ENCLAVE_ID,
     "Invalid enclave identification.",
     NULL},
    {SGX_ERROR_INVALID_SIGNATURE,
     "Invalid enclave signature.",
     NULL},
    {SGX_ERROR_OUT_OF_EPC,
     "Out of EPC memory.",
     NULL},
    {SGX_ERROR_NO_DEVICE,
     "Invalid SGX device.",
     "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."},
    {SGX_ERROR_MEMORY_MAP_CONFLICT,
     "Memory map conflicted.",
     NULL},
    {SGX_ERROR_INVALID_METADATA,
     "Invalid enclave metadata.",
     NULL},
    {SGX_ERROR_DEVICE_BUSY,
     "SGX device was busy.",
     NULL},
    {SGX_ERROR_INVALID_VERSION,
     "Enclave version was invalid.",
     NULL},
    {SGX_ERROR_INVALID_ATTRIBUTE,
     "Enclave was not authorized.",
     NULL},
    {SGX_ERROR_ENCLAVE_FILE_ACCESS,
     "Can't open enclave file.",
     NULL},
    {SGX_ERROR_NDEBUG_ENCLAVE,
     "The enclave is signed as product enclave, and can not be created as debuggable enclave.",
     NULL},
};

const size_t err_length = sizeof sgx_errlist / sizeof sgx_errlist[0];

const char *get_error_message(sgx_status_t ret)
{
    for (int idx = 0; idx < err_length; idx++)
    {
        if (ret == sgx_errlist[idx].err)
        {
            return sgx_errlist[idx].msg;
        }
    }
}

bool check_arr_is_zero(const uint8_t *p, const uint32_t length)
{
    int x = 0;
    for (int i = 0; i < length; i++)
    {
        x |= p[i] ^ 0x0;
    }
    return x == 0 ? true : false;
}