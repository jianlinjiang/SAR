// MIT License

// Copyright (c) 2020 jianlinjiang
#include <assert.h>
#include <map>
#include "enclave_t.h"
#include "enclave_util.h"
#include "sgx_tkey_exchange.h"
#include "sgx_tcrypto.h"
#include "string.h"

typedef struct sgx_aggregation_arguments_t {
  int n; // total_number
  int f; // byzantine number
  int m; // how many selected by krum;
  float r;  // random select 0.1
  int k; // how many selected by sar
}sgx_aggregation_arguments;


// data was allocate outside the dataNode, free was handled by the dataNode
typedef struct dataNode_t
{
  uint8_t *data_;
  uint32_t length_;
  dataNode_t *next_;
  dataNode_t *prev_;
  dataNode_t(uint8_t *data, uint32_t length)
  {
    data_ = data;
    length_ = length;
    next_ = NULL;
    prev_ = NULL;
  }
  ~dataNode_t()
  {
    if (data_)
      free(data_);
    data_ = NULL;
  }
} dataNode;

class dataNodeList
{
public:
  dataNodeList()
  {
    head = new dataNode(NULL, 0);
    tail = new dataNode(NULL, 0);
    head->next_ = tail;
    tail->prev_ = head;
    size = 0;
  }

  ~dataNodeList()
  {
    dataNode *p = head;
    while (p)
    {
      dataNode *tmp = p;
      p = p->next_;
      delete tmp;
    }
  }
  void Insert(uint8_t *data, uint32_t length)
  {
    dataNode *node = new dataNode(data, length);
    dataNode *prev = tail->prev_;
    prev->next_ = node;
    node->next_ = tail;
    node->prev_ = prev;
    tail->prev_ = node;
    size++;
  }

  dataNode* Head() {
    return head->next_;
  }
  
  size_t Size() {
    return size;
  }
private:
  dataNode *head;
  dataNode *tail;
  size_t size = 0;
};

typedef struct key_wrapper_t {
  sgx_ec_key_128bit_t sk_key;
  key_wrapper_t(uint8_t key[16]) {
    for (int i = 0; i < 16; i++) {
      sk_key[i] = key[i];
    }
  }
} key_wrapper;

std::map<int, key_wrapper> g_client_index_key_map;
uint8_t aes_gcm_iv[12] = {0};
int max_num = 1000;
dataNodeList *client_data[1000] = {NULL};

void free_load_weight_context()
{
  for (int i = 0; i < max_num; i++)
  {
    if (client_data[i])
      delete client_data[i];
  }
}

sgx_status_t ecall_create_weights_load_context(sgx_ra_context_t *context_arr, int *index_arr, size_t context_size)
{
  sgx_status_t ret = SGX_SUCCESS;
  int num = context_size / 4;
  for (int i = 0; i < num; i++)
  {
    sgx_ec_key_128bit_t sk_key;
    ret = sgx_ra_get_keys(context_arr[i], SGX_RA_KEY_SK, &sk_key);
    if (SGX_SUCCESS != ret)
    {
      LOG(ERROR, __FILE__, __LINE__, "enclave get keys failed");
      break;
    }
    int client_index = index_arr[i];
    g_client_index_key_map.insert(std::make_pair(client_index, key_wrapper(sk_key)));
  }
  return ret;
}

sgx_status_t ecall_load_weights(uint8_t *p_ciphertext, uint32_t length, uint8_t *p_tag, int client_index)
{
  sgx_status_t ret = SGX_SUCCESS;
  // decrypt the data
  // get key
  auto iter = g_client_index_key_map.find(client_index);
  assert(iter != g_client_index_key_map.end());
  // plaindata buff
  uint8_t *plain_data = (uint8_t *)malloc(length);
  ret = sgx_rijndael128GCM_decrypt(&iter->second.sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t *)p_tag);
  dataNodeList *p_list = NULL;
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "decrypt failed");
    goto cleanup;
  }
  // save the palin data
  
  if (client_data[client_index] == NULL)
  {
    p_list = new dataNodeList();
    client_data[client_index] = p_list;
  }
  else
  {
    dataNodeList *p_list = client_data[client_index];
  }
  
  p_list->Insert(plain_data, length);
cleanup:
  if (ret != SGX_SUCCESS) free_load_weight_context();
  return ret;
}

void ecall_free_load_weight_context() {
  free_load_weight_context();
}

void average_aggregation(int n) {
  dataNode* p_data = client_data[0];
  size_t dataNodeNum = p_data->Size();
  
}

void ecall_aggregation(int layer_index, char aggregation_method, void* arguments, size_t size) {
  sgx_aggregation_arguments* p_args = (sgx_aggregation_arguments*) arguments;
  int n = p_args->n;
  int f = p_args->f;
  int m = p_args->m;
  float r = p_args->r;
  int k = p_args->k;
  switch (aggregation_method)
  {
  case 'a'://average
    average_aggregation(n);
    break;
  case 'm'://median
    break;
  case 't'://trimed mean
    break;
  case 'k':// krum
    break;
  case 's'://sar
    break;
  default:
    LOG(ERROR,__FILE__,__LINE__, "unknown aggregation method");
    break;
  }
}



