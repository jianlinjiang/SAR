/* MIT License 

* Copyright (c) 2020 jianlinjiang

*/
#ifndef ENCLAVE_CONTEXT_H
#define ENCLAVE_CONTEXT_H

#include "util/singleton.h"
#include "ecc_context.h"
namespace sar {
  class EnclaveContext {
    public:
      EnclaveContext();
      ~EnclaveContext();
    private:
      std::unique_ptr<EccContext> ecc_context_ptr_;
  };
}

#endif