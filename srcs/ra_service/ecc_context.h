/* MIT License 

* Copyright (c) 2020 jianlinjiang

*/
#ifndef ECC_CONTEXT_H
#define ECC_CONTEXT_H

#include "sar_util.h"
namespace sar {
  // wrap the ecc context inside the enclave
  // use singleton to call only once and close context automaticly.
  class EccContext {
    public:
      EccContext();
      ~EccContext();
    private:
  };
}

#endif