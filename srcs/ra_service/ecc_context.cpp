/* MIT License 

* Copyright (c) 2020 jianlinjiang

*/

#include "ecc_context.h"

sar::EccContext::EccContext() {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  sgx_status_t retval = SGX_ERROR_UNEXPECTED;
  ret = ecall_enclave_ecc_init(global_eid, &retval);
  if (ret != SGX_SUCCESS || retval != SGX_SUCCESS) {
    LOG(ERROR) << get_error_message(ret);
    LOG(ERROR) << get_error_message(retval);
  }
}

sar::EccContext::~EccContext() {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  sgx_status_t retval = SGX_ERROR_UNEXPECTED;
  ret = ecall_enclave_ecc_shutdown(global_eid, &retval);
  if (ret != SGX_SUCCESS || retval != SGX_SUCCESS) {
    LOG(ERROR) << get_error_message(ret);
    LOG(ERROR) << get_error_message(retval);
  }
}
