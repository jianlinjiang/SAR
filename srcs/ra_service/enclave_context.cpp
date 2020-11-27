/* MIT License 

* Copyright (c) 2020 jianlinjiang

*/
#include "enclave_context.h"
sar::EnclaveContext::EnclaveContext() {
  sgx_status_t ret = SGX_ERROR_UNEXPECTED;
  ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
  if (ret != SGX_SUCCESS) {
    LOG(ERROR) << get_error_message(ret);
  }
  ecc_context_ptr_ = std::move(Singleton<EccContext>::getInstance());
}

sar::EnclaveContext::~EnclaveContext() {
  ecc_context_ptr_.reset();
  sgx_status_t ret = sgx_destroy_enclave(global_eid);
  if (ret != SGX_SUCCESS) {
    LOG(ERROR) << get_error_message(ret);
  }
}
