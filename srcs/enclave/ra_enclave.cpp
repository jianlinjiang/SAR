// MIT License

// Copyright (c) 2020 jianlinjiang

#include <assert.h>
#include "enclave_t.h"
#include "enclave_util.h"
#include "sgx_tkey_exchange.h"
#include "sgx_tcrypto.h"
#include "string.h"

// 
static const sgx_ec256_public_t g_sp_pub_key = {
    {0x72, 0x12, 0x8a, 0x7a, 0x17, 0x52, 0x6e, 0xbf,
     0x85, 0xd0, 0x3a, 0x62, 0x37, 0x30, 0xae, 0xad,
     0x3e, 0x3d, 0xaa, 0xee, 0x9c, 0x60, 0x73, 0x1d,
     0xb0, 0x5b, 0xe8, 0x62, 0x1c, 0x4b, 0xeb, 0x38},
    {0xd4, 0x81, 0x40, 0xd9, 0x50, 0xe2, 0x57, 0x7b,
     0x26, 0xee, 0xb7, 0x41, 0xe7, 0xc6, 0x14, 0xe2,
     0x24, 0xb7, 0xbd, 0xc9, 0x03, 0xf2, 0x9a, 0x28,
     0xa8, 0x3c, 0xc8, 0x10, 0x11, 0x14, 0x5e, 0x06}};
//////////////////////////////////////////////////////////
// global variable
sgx_ecc_state_handle_t g_ecc_handle;
sgx_ec256_public_t g_sar_server_public_key;
sgx_ec256_private_t g_sar_server_private_key;

//////////////////////////////////////////////////////////

// This ecall to generate the key pair inside the enclave.
// TODO: the key should be loaded in a trusted way.
// NOTE: don't generate the enclave key in this way.
sgx_status_t ecall_enclave_ecc_init()
{
  sgx_status_t ret;
  ret = sgx_ecc256_open_context(&g_ecc_handle);
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "sgx ecc 256 open context failed!");
  }
  ret = sgx_ecc256_create_key_pair(&g_sar_server_private_key, &g_sar_server_public_key, g_ecc_handle);
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "sgx ecc 256 create key pair failed!");
  }
  return ret;
}

// This ecall to close the context inside the enclave.
// TODO: the key should be loaded in a trusted way.
// NOTE: don't generate the enclave key in this way.
sgx_status_t ecall_enclave_ecc_shutdown()
{
  sgx_status_t ret;
  ret = sgx_ecc256_close_context(g_ecc_handle);
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "sgx ecc 256 close context failed!");
  }
  return ret;
}

// This ecall is a wrapper of sgx_ra_init to create the trusted
// KE exchange key context needed for the remote attestation
// SIGMA API's.
sgx_status_t ecall_enclave_ra_init(int b_pse, sgx_ra_context_t *p_context, sgx_ec256_public_t *p_client_pub)
{
  sgx_status_t ret;
  ret = sgx_ra_init(p_client_pub, b_pse, p_context);
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "enclave ra context init failed!");
  }
  return ret;
}

// This ecall is a wrapper of sgx_ra_close to create the trusted
// KE exchange key context needed for the remote attestation
// SIGMA API's.
sgx_status_t ecall_enclave_ra_close(sgx_ra_context_t* context)
{
  sgx_status_t ret;
  ret = sgx_ra_close(*context);
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "enclave ra context init failed!");
  }
  return ret;
}
char uint2char(uint8_t x) {
  char y = 0;
  if (x >=0 && x <= 9) {
    y = '0' + x;
  } else {
    y = 'a' + x - 10;
  }
  return y;
} 

void convert(uint8_t src, char &f, char &n) {
  uint8_t fu = src / 16;
  uint8_t fn = src % 16;
  f = uint2char(fu);
  n = uint2char(fn);
}
void print_g_pub_and_pri() {
  char gx[64];
  char c = 0;
  char gy[64];
  char x = 0;
  char r[64];
  for(size_t i = 0 ; i  < 32; i++) {
    convert(g_sar_server_public_key.gx[i], gx[2*i], gx[2*i+1]);
    convert(g_sar_server_public_key.gy[i], gy[2*i], gy[2*i+1]);
    convert(g_sar_server_private_key.r[i], r[2*i], r[2*i+1]);
  }
  LOG(INFO, __FILE__, __LINE__, gx);
  LOG(INFO, __FILE__, __LINE__, gy);
  LOG(INFO, __FILE__, __LINE__, r);
}