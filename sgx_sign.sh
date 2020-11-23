#!/bin/bash
/opt/intel/sgxsdk/bin/x64/sgx_sign gendata -enclave enclave.so -config Enclave/Enclave.config.xml -out enclave_hash.hex
openssl dgst -sha256 -out signature.hex -sign private_key.pem -keyform PEM enclave_hash.hex
/opt/intel/sgxsdk/bin/x64/sgx_sign catsig -enclave enclave.so -config Enclave/Enclave.config.xml -out enclave.signed.so -key  public_key.pem  -sig signature.hex  -unsigned enclave_hash.hex