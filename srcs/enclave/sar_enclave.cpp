// MIT License

// Copyright (c) 2020 jianlinjiang
#include <assert.h>
#include <map>
#include <vector>
#include <algorithm>
#include <functional>
#include <numeric>
#include <random>
#include <unordered_set>
#include <queue>
#include "math.h"
#include "float.h"
#include "enclave_t.h"
#include "enclave_util.h"
#include "sgx_tkey_exchange.h"
#include "sgx_tcrypto.h"
#include "string.h"

typedef struct sgx_aggregation_arguments_t {
  char a; // aggregation method
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
    head = new dataNode(nullptr, 0);
    tail = new dataNode(nullptr, 0);
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
      if (tmp->data_ != nullptr) {
        free(tmp->data_);
        tmp->data_ = nullptr;
      }
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

  dataNode* Get(size_t index) {
    if (index >= size) {
      LOG(ERROR, __FILE__, __LINE__, "list index out of bound size")
      return NULL;
    }
    dataNode* res = head->next_;
    while(index--) {
      res = res->next_;
    }
    return res;
  }
private:
  dataNode *head;
  dataNode *tail;
  size_t size = 0;
};

class pair_heap {
  // max heap
  public:
    pair_heap(int c) : k(c) {
      heap = std::vector<std::pair<double, int>>(c+1, std::make_pair(0.0, 0));
    }
    void InitHeap(int i, double score, int index) {
      if (i>k) {
        return ;
      }
      heap[i] = std::make_pair(score, index);
    }
    void MakeHeap() {
      for(int i = k / 2; i>= 1; i--) down(i);
    }
    void Push(double score, int index) {
      if (heap[1].first > score) {
        heap[1].first = score;
        heap[1].second = index;
        down(1);
      }
    }
    int GetIndex(int i) { //start from 1
      return heap[i+1].second;
    }
  private:
    // index start from 1
    void down(int u) {
      int t = u;
      if (u*2 <= k && heap[u*2].first > heap[t].first) t = u*2;
      if (u*2+1 <= k && heap[u*2+1].first > heap[t].first) t = u*2+1;
      if (u != t) {
        swap(heap[u], heap[t]);
        down(t);
      }
    }
    void up(int u) {
      while(u/2 && heap[u/2].first < heap[u].first) {
        swap(heap[u/2] , heap[u]);
        u/=2;
      }
    }
    int k ;
    std::vector<std::pair<double, int>> heap;
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
int client_num;
dataNodeList *client_data[1050] = {nullptr};
// save as 
// datanode, weight_num client_num
std::vector<std::vector<float*>> weights_arr;
std::vector<float*> weights_arr2;
std::vector<size_t> weights_num2; 
std::vector<float*> aggregation_result;
std::vector<uint8_t*> encrypt_aggregation_result;
std::vector<int> global_selected;
// node weight
void free_load_weight_context()
{
  for (int i = 0; i < client_num; i++)
  {
    if (client_data[i])
      delete client_data[i];
    client_data[i] = nullptr;
  }
}

void free_load_weight_context_optimized2() {
  while (!weights_arr2.empty())
  {
    float* weight = weights_arr2.back();
    if (weight) free(weight);
    weights_arr2.pop_back();
  }
  while(!aggregation_result.empty()){
    float* result = aggregation_result.back();
    if (result) free(result);
    aggregation_result.pop_back();
  }
  while(!encrypt_aggregation_result.empty()){
    uint8_t* encrypt_result = encrypt_aggregation_result.back();
    if (encrypt_result) free(encrypt_result);
    encrypt_aggregation_result.pop_back();
  }
  weights_num2.clear();
}

void free_load_weight_context_optimized() {
  while(!weights_arr.empty()) {
    std::vector<float*> column = weights_arr.back();
    for(int i = 0; i < column.size(); i++) {
      if (column[i]) free(column[i]);
    }
    weights_arr.pop_back();
  }
  while(!aggregation_result.empty()){
    float* result = aggregation_result.back();
    if (result) free(result);
    aggregation_result.pop_back();
  }
  while(!encrypt_aggregation_result.empty()){
    uint8_t* encrypt_result = encrypt_aggregation_result.back();
    if (encrypt_result) free(encrypt_result);
    encrypt_aggregation_result.pop_back();
  }
}

sgx_status_t ecall_create_weights_load_context(sgx_ra_context_t *context_arr, int *index_arr, size_t context_size)
{
  sgx_status_t ret = SGX_SUCCESS;
  int num = context_size / 4;
  client_num = num;
  for (int i = 0; i < num; i++)
  {
    sgx_ec_key_128bit_t sk_key;
    ret = sgx_ra_get_keys(context_arr[i], SGX_RA_KEY_SK, &sk_key);
    if (SGX_SUCCESS != ret)
    {
      LOG(ERROR, __FILE__, __LINE__, "enclave get keys failed");
      break;
    }
    int index = index_arr[i];
    g_client_index_key_map.insert(std::make_pair(index, key_wrapper(sk_key)));
  }
  return ret;
}



sgx_status_t ecall_load_weights_optimized2(uint8_t *p_ciphertext, uint32_t length, uint8_t* p_tag, int index, int node_index) {
  sgx_status_t ret = SGX_SUCCESS;
  auto iter = g_client_index_key_map.find(index);
  assert(iter != g_client_index_key_map.end());
  uint8_t *plain_data = (uint8_t*)malloc(length);
  // only for test 
  // 
  sgx_ec_key_128bit_t sk_key;
  memset(sk_key, 0x0, SGX_CMAC_KEY_SIZE);
  ret = sgx_rijndael128GCM_decrypt(&sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t*)p_tag);
  uint32_t weight_num = length / sizeof(float);
  float* plain_weight = (float*) plain_data;
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "decrypt failed ");
    if (ret == SGX_ERROR_INVALID_PARAMETER) {
      LOG(ERROR, __FILE__, __LINE__, "invalid parameter");
    }
    if (ret == SGX_ERROR_MAC_MISMATCH) {
      LOG(ERROR, __FILE__, __LINE__, "The input MAC does not match the MAC calculated.");
    }
    if (ret == SGX_ERROR_OUT_OF_MEMORY) {
      LOG(ERROR, __FILE__, __LINE__, "Not enough memory is available to complete this operation.");
    }
    if (ret == SGX_ERROR_UNEXPECTED) {
      LOG(ERROR, __FILE__, __LINE__, "An internal cryptography library failure occurred.");
    }
    goto cleanup;
  }
  
  if (index == 0) {
    float* weights = (float*) malloc(sizeof(float)*weight_num*client_num);
    weights_arr2.push_back(weights);
    weights_num2.push_back(weight_num);
    float* result_data = (float*)malloc(sizeof(float)*weight_num);
    uint8_t* encrypt_result_data = (uint8_t*) malloc(sizeof(float)*weight_num);
    aggregation_result.push_back(result_data);
    encrypt_aggregation_result.push_back(encrypt_result_data);
  }
  for(int i = 0; i < weight_num; i++) {
    weights_arr2[node_index][i*client_num+index] = plain_weight[i];
  }
  cleanup:
    if (plain_data) free(plain_data);
    if (ret != SGX_SUCCESS) {
      free_load_weight_context_optimized2();
    }
    return ret;
}

void ecall_create_weights_load_context3(int n)
{
  client_num = n;
}

// client index
sgx_status_t ecall_load_weights_optimized3(uint8_t *p_ciphertext, uint32_t length, uint8_t* p_tag, int index, int node_index) {
  sgx_status_t ret = SGX_SUCCESS;
  // decrypt the data
  // get key
  uint8_t *plain_data = (uint8_t*)malloc(length);
  sgx_ec_key_128bit_t sk_key;
  memset(sk_key, 0x0, SGX_CMAC_KEY_SIZE);
  ret = sgx_rijndael128GCM_decrypt(&sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t*)p_tag);

  uint32_t weight_num = length / sizeof(float);
  float* plain_weight = (float*) plain_data;
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "decrypt failed ");
    if (ret == SGX_ERROR_INVALID_PARAMETER) {
      LOG(ERROR, __FILE__, __LINE__, "invalid parameter");
    }
    if (ret == SGX_ERROR_MAC_MISMATCH) {
      LOG(ERROR, __FILE__, __LINE__, "The input MAC does not match the MAC calculated.");
    }
    if (ret == SGX_ERROR_OUT_OF_MEMORY) {
      LOG(ERROR, __FILE__, __LINE__, "Not enough memory is available to complete this operation.");
    }
    if (ret == SGX_ERROR_UNEXPECTED) {
      LOG(ERROR, __FILE__, __LINE__, "An internal cryptography library failure occurred.");
    }
    goto cleanup;
  }
  
  if (index == 0 ) {
    // first time allocate memory
    std::vector<float*> column(weight_num, nullptr);
    for (int i = 0; i < weight_num; i++) {
      column[i] = (float*) malloc(sizeof(float) * client_num); 
    }
    weights_arr.push_back(column);
    float* result_data = (float*)malloc(sizeof(float) * weight_num);
    uint8_t* encrypt_result_data = (uint8_t*) malloc(sizeof(float) * weight_num);
    aggregation_result.push_back(result_data);
    encrypt_aggregation_result.push_back(encrypt_result_data);
  }
  for(int i = 0; i < weight_num; i++) {
    weights_arr[node_index][i][index] = plain_weight[i];
  } 

  cleanup:
    if (plain_data) free(plain_data);
    if (ret != SGX_SUCCESS) {
      free_load_weight_context_optimized();
    }
    return ret;
}

// client index
sgx_status_t ecall_load_weights_optimized(uint8_t *p_ciphertext, uint32_t length, uint8_t* p_tag, int index, int node_index) {
  sgx_status_t ret = SGX_SUCCESS;
  // decrypt the data
  // get key
  auto iter = g_client_index_key_map.find(index);
  assert(iter != g_client_index_key_map.end());
  uint8_t *plain_data = (uint8_t*)malloc(length);
  ret = sgx_rijndael128GCM_decrypt(&iter->second.sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t*)p_tag);
  sgx_ec_key_128bit_t sk_key;
  memset(sk_key, 0x0, SGX_CMAC_KEY_SIZE);
  ret = sgx_rijndael128GCM_decrypt(&sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t*)p_tag);

  uint32_t weight_num = length / sizeof(float);
  float* plain_weight = (float*) plain_data;
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "decrypt failed ");
    if (ret == SGX_ERROR_INVALID_PARAMETER) {
      LOG(ERROR, __FILE__, __LINE__, "invalid parameter");
    }
    if (ret == SGX_ERROR_MAC_MISMATCH) {
      LOG(ERROR, __FILE__, __LINE__, "The input MAC does not match the MAC calculated.");
    }
    if (ret == SGX_ERROR_OUT_OF_MEMORY) {
      LOG(ERROR, __FILE__, __LINE__, "Not enough memory is available to complete this operation.");
    }
    if (ret == SGX_ERROR_UNEXPECTED) {
      LOG(ERROR, __FILE__, __LINE__, "An internal cryptography library failure occurred.");
    }
    goto cleanup;
  }
  
  if (index == 0 ) {
    // first time allocate memory
    std::vector<float*> column(weight_num, nullptr);
    for (int i = 0; i < weight_num; i++) {
      column[i] = (float*) malloc(sizeof(float) * client_num); 
    }
    weights_arr.push_back(column);
    float* result_data = (float*)malloc(sizeof(float) * weight_num);
    uint8_t* encrypt_result_data = (uint8_t*) malloc(sizeof(float) * weight_num);
    aggregation_result.push_back(result_data);
    encrypt_aggregation_result.push_back(encrypt_result_data);
  }
  for(int i = 0; i < weight_num; i++) {
    weights_arr[node_index][i][index] = plain_weight[i];
  } 

  cleanup:
    if (plain_data) free(plain_data);
    if (ret != SGX_SUCCESS) {
      free_load_weight_context_optimized();
    }
    return ret;
}

sgx_status_t ecall_load_weights(uint8_t *p_ciphertext, uint32_t length, uint8_t *p_tag, int index)
{
  sgx_status_t ret = SGX_SUCCESS;
  // decrypt the data
  // get key
  auto iter = g_client_index_key_map.find(index);
  assert(iter != g_client_index_key_map.end());
  // plaindata buff
  uint8_t *plain_data = (uint8_t *)malloc(length);
  ret = sgx_rijndael128GCM_decrypt(&iter->second.sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t *)p_tag);
  dataNodeList *p_list = NULL;
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "decrypt failed ");
    if (ret == SGX_ERROR_INVALID_PARAMETER) {
      LOG(ERROR, __FILE__, __LINE__, "invalid parameter");
    }
    if (ret == SGX_ERROR_MAC_MISMATCH) {
      LOG(ERROR, __FILE__, __LINE__, "The input MAC does not match the MAC calculated.");
    }
    if (ret == SGX_ERROR_OUT_OF_MEMORY) {
      LOG(ERROR, __FILE__, __LINE__, "Not enough memory is available to complete this operation.");
    }
    if (ret == SGX_ERROR_UNEXPECTED) {
      LOG(ERROR, __FILE__, __LINE__, "An internal cryptography library failure occurred.");
    }
    goto cleanup;
  }
  // save the palin data
  
  if (client_data[index] == nullptr)
  {
    p_list = new dataNodeList();
    client_data[index] = p_list;
  }
  else
  {
    p_list = client_data[index];
  }
  
  p_list->Insert(plain_data, length);
cleanup:
  if (ret != SGX_SUCCESS) free_load_weight_context();
  return ret;
}

sgx_status_t ecall_load_weights2(uint8_t *p_ciphertext, uint32_t length, uint8_t *p_tag, int index)
{
  sgx_status_t ret = SGX_SUCCESS;
  // decrypt the data
  // get key
  sgx_ec_key_128bit_t sk_key;
  memset(sk_key, 0x0, SGX_CMAC_KEY_SIZE);
  // plaindata buff
  uint8_t *plain_data = (uint8_t *)malloc(length);
  ret = sgx_rijndael128GCM_decrypt(&sk_key, p_ciphertext, length, plain_data, aes_gcm_iv, 12, NULL, 0, (const sgx_aes_gcm_128bit_tag_t *)p_tag);
  dataNodeList *p_list = NULL;
  if (ret != SGX_SUCCESS)
  {
    LOG(ERROR, __FILE__, __LINE__, "decrypt failed ");
    if (ret == SGX_ERROR_INVALID_PARAMETER) {
      LOG(ERROR, __FILE__, __LINE__, "invalid parameter");
    }
    if (ret == SGX_ERROR_MAC_MISMATCH) {
      LOG(ERROR, __FILE__, __LINE__, "The input MAC does not match the MAC calculated.");
    }
    if (ret == SGX_ERROR_OUT_OF_MEMORY) {
      LOG(ERROR, __FILE__, __LINE__, "Not enough memory is available to complete this operation.");
    }
    if (ret == SGX_ERROR_UNEXPECTED) {
      LOG(ERROR, __FILE__, __LINE__, "An internal cryptography library failure occurred.");
    }
    goto cleanup;
  }
  // save the palin data
  
  if (client_data[index] == nullptr)
  {
    p_list = new dataNodeList();
    client_data[index] = p_list;
  }
  else
  {
    p_list = client_data[index];
  }
  
  p_list->Insert(plain_data, length);
cleanup:
  if (ret != SGX_SUCCESS) free_load_weight_context();
  return ret;
}

void ecall_free_load_weight_context() {
  free_load_weight_context();
}

void ecall_free_load_weight_context_optimized2() {
  free_load_weight_context_optimized2();
}

void ecall_free_load_weight_context_optimized() {
  free_load_weight_context_optimized();
}

void average_aggregation_optimized(int n) {
  int node_num = weights_arr.size();
  for(int i = 0; i < node_num; i++) {
    std::vector<float*> column = weights_arr[i];
    for(int j = 0; j< column.size(); j++) {
      float *weights = column[j];
      // weight is a arr contain n weights
      float result = 0;
      for(int z = 0; z < n; z++) {
        result += weights[z];
      }
      result /= n;
      aggregation_result[i][j] = result;
    }
  }
}

void average_aggregation_optimized2(int n) {
  int node_num = weights_arr2.size();
  for(int i = 0; i < node_num; i++){
    float* weight = weights_arr2[i];
    size_t num = weights_num2[i];
    for(unsigned int j = 0; j < num; j++){
      float result = 0;
      for(unsigned int k = 0; k < client_num; k++) {
        result += weight[j*client_num+k];
      }
      result /= n;
      aggregation_result[i][j] = result;
    }
  }
}
// client 0 as the result
void average_aggregation(int n) {
  size_t dataNodeNum = client_data[0]->Size();
  for (int k = 0; k < dataNodeNum; k ++) {
    uint32_t length = client_data[0]->Get(k)->length_;
    size_t weights_num = length / sizeof(float); 
    float* res = (float*) client_data[client_num]->Get(k)->data_;
    for (int i = 0; i < n; i ++) {
      dataNode* p_data = client_data[i]->Get(k);
      float* p_weights = (float*)p_data->data_;
      for(int  j = 0; j < weights_num; j++) {
        res[j] += p_weights[j];
      }
    }
    for (int j = 0; j < weights_num; j++) {
      res[j] /= n;
    }
  }
}

void trimed_mean_aggregation(int n, int f) {
  size_t dataNodeNum = client_data[0]->Size();
  for (int k = 0; k < dataNodeNum; k++) {
    uint32_t length = client_data[0]->Get(k)->length_;
    size_t weights_num = length / sizeof(float);
    std::vector<float> weights(n, 0);
    float* result_data = (float*) client_data[client_num]->Get(k)->data_;
    for (int i = 0; i < weights_num; i++) {
      for(int j = 0; j < n; j++) {
        float* weight_data = ((float*)client_data[j]->Get(k)->data_)+i;
        weights[j] = *weight_data;
      }
      float result = 0;
      // assert n > 2*f 
      std::sort(weights.begin(), weights.end());
      result = std::accumulate(weights.begin()+f, weights.end()-f, 0);
      result /= (n - 2 * f); 
      result_data[i] = result;
    }
  }
}

void trimed_mean_aggregation_optimized(int n, int f) {
  int dataNodeNum = weights_arr.size();
  for(int i = 0; i < dataNodeNum; i++) {
    std::vector<float*> column = weights_arr[i];
    for (int j = 0; j < column.size(); j++){
      // trimed mean
      float* weights = column[j];
      std::sort(weights, weights+n);
      float result = 0;
      result = std::accumulate(weights+f, weights+n-f, 0);
      result /= (n-2*f);
      aggregation_result[i][j] = result;
    }
  }
}

void trimed_mean_aggregation_optimized2(int n, int f) {
  int node_num = weights_arr2.size();
  for(int i = 0; i < node_num; i++) {
    float* weights = weights_arr2[i];
    size_t weight_num = weights_num2[i];
    for(int j = 0; j < weight_num; j++){
      float* start = weights + j * n;
      std::sort(start, start+n);
      float result = 0;
      result = std::accumulate(start+f,start+n-f,0);
      result /= (n-2*f);
      aggregation_result[i][j] = result;
    }
  }
}

void median_aggregation_optimized2(int n) {
  int node_num = weights_arr2.size();
  for(int i =0; i < node_num; i++) {
    float* weights = weights_arr2[i];
    size_t weight_num = weights_num2[i];
    for(int j = 0; j < weight_num; j++){
      float* start = weights+j*client_num;
      std::nth_element(start,start+client_num/2,start+client_num);
      float result = start[n/2];
      aggregation_result[i][j] = result;
    }
  }
}

void median_aggregation_optimized(int n) {
  int node_num = weights_arr.size();
  for (int i = 0; i < node_num; i++) {
    std::vector<float*> column = weights_arr[i];
    for(int j = 0; j < column.size(); j++) {
      float *weights = column[j];
      float result = 0;
      // if (n % 2 == 0) {
      //   std::nth_element(weights, weights+n/2, weights+n);
      //   result += weights[n/2];
      //   std::nth_element(weights, weights+n/2-1,weights+n);
      //   result += weights[n/2+1];
      //   result /= 2;
      // } else {
        std::nth_element(weights, weights + n / 2, weights + n);
        result += weights[n/2];
      // }
      aggregation_result[i][j] = result;
    }
  }
}

void median_aggregation_straightforward(int n) {
  size_t dataNodeNum = client_data[0]->Size();
  for (int k = 0; k < dataNodeNum; k++) {
    uint32_t length = client_data[0]->Get(k)->length_;
    size_t weights_num = length / sizeof(float);
    std::vector<float> weights(n, 0);
    float *result_data = (float*)client_data[client_num]->Get(k)->data_;
    for(int i = 0; i < weights_num; i++) { 
      for(int j = 0; j < n; j++) {
        float* weight_data = ((float*)client_data[j]->Get(k)->data_) + i;
        weights[j] = *weight_data;
      }
      float result = 0;
      // if (n % 2 == 0) {
      //   // even
      //   std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
      //   result = weights[weights.size()/2];
      //   std::nth_element(weights.begin(), weights.begin()+weights.size()/2-1, weights.end());
      //   result += weights[weights.size()/2-1];
      //   result /= 2;
      // } else {
        // odd 
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
        result = weights[weights.size()/2];
      // }
      result_data[i] = result;
    }
  }
}
std::unordered_set<int> pickSet(int N, int k, std::mt19937& gen)
{
    std::unordered_set<int> elems;
    for (int r = N - k; r < N; ++r) {
        int v = std::uniform_int_distribution<>(1, r)(gen);
        // there are two cases.
        // v is not in candidates ==> add it
        // v is in candidates ==> well, r is definitely not, because
        // this is the first iteration in the loop that we could've
        // picked something that big.
        if (!elems.insert(v).second) {
            elems.insert(r);
        }   
    }
    return elems;
}


std::vector<int> pick(int N, int k) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::unordered_set<int> elems = pickSet(N, k, gen);
    // ok, now we have a set of k elements. but now
    // it's in a [unknown] deterministic order.
    // so we have to shuffle it:
    std::vector<int> result(elems.begin(), elems.end());
    return result;
}

void ecall_random_generate(int total, int select_num) {
  global_selected = pick(total, select_num);
}

void sar_aggregation(int n, int f, float r, int k) {
  size_t dataNodeNum = client_data[0]->Size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for (int i = 0; i < n; i++) {
    for (int j = i+1; j < n; j++) {
      double d = 0;
      for (int x = 0; x < dataNodeNum; x++) {
        uint32_t weights_num = client_data[0]->Get(x)->length_ / sizeof(float);
        std::vector<int> selected;
        for(int index : global_selected) {
          if (index < weights_num) {
            selected.push_back(index);
          }
        }
        float* weight_i = (float*) client_data[i]->Get(x)->data_;
        float* weight_j = (float*) client_data[j]->Get(x)->data_;
        for (int z = 0; z < selected.size(); z++) {
          d += (weight_i[selected[z]] - weight_j[selected[z]]) * (weight_i[selected[z]] - weight_j[selected[z]]);
        }
      }
      distance[i][j] = sqrt(d);
    }
  }
  // transpose
  for(int i = 0; i < n ; i++) {
    for (int j = i - 1; j >= 0; j--) {
      distance[i][j] = distance[j][i];
    }
  }
  // sum the closest n-f-2 distances
  std::vector<double> scores(n, 0);
  for(int i = 0; i < n; i++) {
    std::sort(distance[i].begin(), distance[i].end());
    scores[i] = std::accumulate(distance[i].begin(), distance[i].begin()+n-f-2+1, 0);
  }
  // choose the topk
  auto cmp = [](std::pair<double, int> p1, std::pair<double, int> p2) {
    if (p1.first == p2.first) {
      return p1.second < p2.second;
    }
    return p1.first < p2.first;
  };
  std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, decltype(cmp)> queue(cmp);
  for (int i = 0; i < n; i ++) {
    queue.push(std::make_pair(scores[i], i));
  }
  std::vector<int> indexs;
  for(int i = 0; i < k; i++) {
    indexs.push_back(queue.top().second);
    queue.pop();
  }
  // calculate the median of the top k weights
  for(int x = 0; x < dataNodeNum; x++) {
    size_t weight_num = client_data[0]->Get(x)->length_ / sizeof(float);
    std::vector<float> weights(k, 0);
    // the data is stored in client_data[client_num]
    float* result_data = (float*) client_data[client_num]->Get(x)->data_;
    for(int i = 0; i < weight_num; i++) {
      for (int j = 0; j < k; j ++){
        int index = indexs[j];
        float* weight_data = ((float*)client_data[index]->Get(x)->data_)+i;
        weights[j] = *weight_data;
      }
      float result = 0;
      // if (k % 2 == 0) {
      //   std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
      //   result = weights[weights.size()/2];
      //   std::nth_element(weights.begin(), weights.begin()+weights.size()/2-1, weights.end());
      //   result += weights[weights.size()/2-1];
      //   result /= 2;
      // } else {
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
        result = weights[weights.size()/2];
      // }
      result_data[i] = result;
    }
  }
}

void krum_aggregation_optimized2(int n, int f, int m) {
  int node_num = weights_arr2.size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for(int i = 0; i < node_num; i++) {
    float* weights = weights_arr2[i];
    size_t weights_num = weights_num2[i];
    for(int j = 0; j < weights_num; j++) {
      float* start = weights + j * n;
      for(int x = 0; x < n; x ++) {
        for(int y = x + 1; y < n; y ++) {
          distance[x][y] += (start[x]-start[y]) * (start[x]-start[y]);
        }
      }
    }
  }
  // transpose 
  for(int i = 0; i < n; i++) {
    for(int j = i - 1; j>= 0; j--) {
      distance[j][i] = sqrt(distance[j][i]);
      distance[i][j] = distance[j][i];
    }
  }
  std::vector<double> scores(n, 0);
  for(int i = 0; i < n; i++) {
    std::sort(distance[i].begin(), distance[i].end());
    scores[i] = std::accumulate(distance[i].begin(), distance[i].begin()+n-f-2+1, 0);
  }
  // find the smallest scores index
  int index = -1;
  double smallest = DBL_MAX;
  for(int i = 0; i < n; i++) {
    if (scores[i] < smallest) {
      smallest = scores[i];
      index = i;
    }
  }
  // save the result to aggregation result 
  for (int i = 0; i < node_num; i++) {
    float* weights = weights_arr2[i];
    size_t weights_num = weights_num2[i];
    for (int j = 0; j < weights_num; j++) {
      float* start = weights + j * n;
      aggregation_result[i][j] = start[index];
    }
  }
}


void krum_aggregation_optimized(int n, int f, int m) {
  int dataNodeNum = weights_arr.size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for (int i = 0; i < dataNodeNum; i++) {
    std::vector<float*> column = weights_arr[i];
    for(int j = 0; j < column.size(); j++) {
      float* weights = column[j];
      for (int x = 0; x < n; x++) {
        for (int y = x + 1; y < n; y++) {
          distance[x][y] += (weights[x] - weights[y])*(weights[x] - weights[y]);
        }
      }
    }
  }
  // transpose 
  for(int i = 0; i < n; i++) {
    for(int j = i - 1; j>= 0; j--) {
      distance[j][i] = sqrt(distance[j][i]);
      distance[i][j] = distance[j][i];
    }
  }
  // sum the closeest n-f-2 distances
  std::vector<double> scores(n, 0);
  for(int i = 0; i < n; i++) {
    std::sort(distance[i].begin(), distance[i].end());
    scores[i] = std::accumulate(distance[i].begin(), distance[i].begin()+n-f-2+1, 0);
  }
  // find the smallest scores index
  int index = -1;
  double smallest = DBL_MAX;
  for(int i = 0; i < n; i++) {
    if (scores[i] < smallest) {
      smallest = scores[i];
      index = i;
    }
  }
  // save the result to aggregation result 
  for (int i = 0; i < dataNodeNum; i++) {
    std::vector<float*> column = weights_arr[i];
    for (int j = 0; j < column.size(); j++) {
      aggregation_result[i][j] = column[j][index];
    }
  }
}

void sar_aggregation_optimized2(int n, int f, float r, int k ){
  int node_num = weights_arr2.size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for (int i = 0; i < weights_arr2.size(); i++) {
    float* weights = weights_arr2[i];
    size_t weights_num = weights_num2[i];
    uint32_t select_num = (uint32_t)weights_num*r;
    std::vector<int> selected = pick(weights_num, select_num);
    for(int z = 0; z < selected.size(); z++) {
      float* start = weights + selected[z] * n;
      for(int x = 0; x < n; x++) {
        for(int y = x + 1; y < n; y++) {
          distance[x][y] += (start[x]-start[y])*(start[x]-start[y]);
        }
      }
    }
  }
  // transpose
  for (int i = 0; i < n; i++) {
    for(int j = i-1; j>=0; j--) {
      distance[j][i] = sqrt(distance[j][i]);
      distance[i][j] = distance[j][i];
    }
  }
  // sum the closest n - f - 2 distance
  std::vector<double> scores(n, 0);
  for (int i = 0; i< n; i++) {
    std::sort(distance[i].begin(), distance[i].end());
    scores[i] = std::accumulate(distance[i].begin(), distance[i].begin()+n-f-2+1, 0);
  }
  // choose the topk with the smallest scores
  auto cmp = [](std::pair<double, int> p1, std::pair<double, int> p2) {
    if (p1.first == p2.first) {
      return p1.second < p2.second;
    }
    return p1.first < p2.first;
  };
  std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, decltype(cmp)> queue(cmp);
  for (int i = 0; i < n; i ++) {
    queue.push(std::make_pair(scores[i], i));
  }
  std::vector<int> topk_indexs;
  for(int i = 0; i < k; i++) {
    topk_indexs.push_back(queue.top().second);
    queue.pop();
  }
  // calculate the median of the top k weights
  std::vector<float> topk_weights(k,0);

  for (int i = 0; i < node_num; i++) {
    float* weights = weights_arr2[i];
    size_t weights_num = weights_num2[i];

    for (int j = 0; j < weights_num; j++) {
      float* start = weights + j * n;
      for(int z = 0; z < k; z++) {
        int topk_index = topk_indexs[z];
        topk_weights[z] = start[topk_index];
      }
      std::nth_element(topk_weights.begin(), topk_weights.begin()+topk_weights.size()/2,topk_weights.end());
      aggregation_result[i][j] = topk_weights[topk_weights.size()/2];
    }
  }
}

void sar_aggregation_optimized(int n, int f, float r, int k) {
  int dataNodeNum = weights_arr.size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for(int i = 0; i < dataNodeNum; i++) {
    // each column
    std::vector<float*> column = weights_arr[i];
    int weight_num = column.size();
    std::vector<int> selected;
    for(int index : global_selected) {
      if (index < weight_num) {
        selected.push_back(index);
      }
    }
    for (int z = 0; z < selected.size(); z++) {
      float* weights = column[selected[z]];
      for (int x = 0; x < n; x ++ ){
        for(int y = x + 1; y < n; y++) {
          distance[x][y] += (weights[x] - weights[y])* (weights[x] - weights[y]);
        }
      }
    }
  }
  // transpose
  for (int i = 0; i < n; i++) {
    for(int j = i-1; j>=0; j--) {
      distance[j][i] = sqrt(distance[j][i]);
      distance[i][j] = distance[j][i];
    }
  }
  // sum the closest n - f - 2 distance
  std::vector<double> scores(n, 0);
  for (int i = 0; i< n; i++) {
    std::sort(distance[i].begin(), distance[i].end());
    scores[i] = std::accumulate(distance[i].begin(), distance[i].begin()+n-f-2+1, 0);
  }
  // choose the topk with the smallest scores
  auto cmp = [](std::pair<double, int> p1, std::pair<double, int> p2) {
    if (p1.first == p2.first) {
      return p1.second < p2.second;
    }
    return p1.first < p2.first;
  };
  std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, decltype(cmp)> queue(cmp);
  for (int i = 0; i < n; i ++) {
    queue.push(std::make_pair(scores[i], i));
  }
  std::vector<int> topk_indexs;
  for(int i = 0; i < k; i++) {
    topk_indexs.push_back(queue.top().second);
    queue.pop();
  }
  // calculate the median of the top k weights
  std::vector<float> topk_weights(k, 0);
  for (int i = 0; i < dataNodeNum; i++) {
    std::vector<float*> column = weights_arr[i];
    for(int j = 0; j < column.size(); j++) {
      float* weights = column[j];
      for(int z = 0; z < k; z ++) {
        int topk_index = topk_indexs[z];
        topk_weights[z] = weights[topk_index];
      }
      float result = 0; 
      // if(k % 2 == 0) {
      //   std::nth_element(topk_weights.begin(), topk_weights.begin()+topk_weights.size()/2, topk_weights.end());
      //   result += topk_weights[topk_weights.size()/2];
      //   std::nth_element(topk_weights.begin(),topk_weights.begin()+topk_weights.size()/2-1, topk_weights.end());
      //   result += topk_weights[topk_weights.size()/2-1];
      //   result /= 2;
      // } else {
        std::nth_element(topk_weights.begin(), topk_weights.begin()+topk_weights.size()/2, topk_weights.end());
        result = topk_weights[topk_weights.size()/2];
      // }
      aggregation_result[i][j] = result;
    }
  }
}

void krum_aggregation(int n, int f, int m) {
  size_t dataNodeNum = client_data[0]->Size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for (int i = 0; i < n; i++) {
    for (int j = i+1; j < n; j ++){
      double d = 0;
      for (int k = 0; k < dataNodeNum; k++) {
        size_t weights_num = client_data[0]->Get(k)->length_/sizeof(float);
        float* weight_i = (float*)client_data[i]->Get(k)->data_;
        float* weight_j = (float*)client_data[j]->Get(k)->data_;
        for(int z = 0; z < weights_num; z++) {
          d += (weight_i[z] - weight_j[z])*(weight_i[z] - weight_j[z]);
        }
      }
      distance[i][j] = sqrt(d);
    }
  }
  // transpose
  for(int i = 0; i < n ; i++) {
    for (int j = i - 1; j >= 0; j--) {
      distance[i][j] = distance[j][i];
    }
  }
  // sum the closest n-f-2 distances
  std::vector<double> scores(n, 0);
  for(int i = 0; i < n; i++) {
    std::sort(distance[i].begin(), distance[i].end());
    scores[i] = std::accumulate(distance[i].begin(), distance[i].begin()+n-f-2+1, 0);
  }
  // find the smallest scores index;
  int index = -1;
  double smallest = DBL_MAX;
  for (int i = 0; i < n; i++) {
    if (scores[i] < smallest) {
      smallest = scores[i];
      index = i;
    }
  }
  // copy client i's data to client[client_num]
  // swap client0 and client i
  for (int i = 0; i < dataNodeNum; i++) {
    memcpy(client_data[client_num]->Get(i)->data_, client_data[index]->Get(i)->data_, client_data[index]->Get(i)->length_);
  }
}

void ecall_aggregation_optimized2(int layer_index, void* arguments, size_t size ) {
  sgx_aggregation_arguments* p_args = (sgx_aggregation_arguments*) arguments;
  char a = p_args->a;
  int n = p_args->n;
  int f = p_args->f;
  int m = p_args->m;
  float r = p_args->r;
  int k = p_args->k;
  switch (a)
  {
  case 'a'://average
    average_aggregation_optimized2(n);
    break;
  case 'm'://median
    median_aggregation_optimized2(n);
    break;
  case 't'://trimed mean
    trimed_mean_aggregation_optimized2(n, f);
    break;
  case 'k':// krum
    krum_aggregation_optimized2(n, f, m);
    break;
  case 's'://sar
    sar_aggregation_optimized2(n, f, r, k);
    break;
  default:
    LOG(ERROR,__FILE__,__LINE__, "unknown aggregation method");
    break;
  }
  uint8_t encrypt_key[16] = {0x0};
  sgx_aes_gcm_128bit_tag_t tag;
  int dataNodeNum = weights_arr2.size();
  for(int i = 0; i < dataNodeNum; i++) {
    sgx_status_t ret = sgx_rijndael128GCM_encrypt((const sgx_aes_gcm_128bit_key_t*)encrypt_key, (uint8_t*)aggregation_result[i], weights_num2[i] * sizeof(float), encrypt_aggregation_result[i], aes_gcm_iv, 12, NULL, 0, &tag);
    if (ret != SGX_SUCCESS) {
      LOG(ERROR, __FILE__, __LINE__, "aes gcm encrypt failed");
      return ;
    }
    ocall_save_aggregation_result(layer_index, encrypt_aggregation_result[i], weights_num2[i] * sizeof(float),tag);
  }
}


void ecall_aggregation_optimized(int layer_index, void* arguments, size_t size) {
  sgx_aggregation_arguments* p_args = (sgx_aggregation_arguments*) arguments;
  char a = p_args->a;
  int n = p_args->n;
  int f = p_args->f;
  int m = p_args->m;
  float r = p_args->r;
  int k = p_args->k;
  switch (a)
  {
  case 'a'://average
    average_aggregation_optimized(n);
    break;
  case 'm'://median
    median_aggregation_optimized(n);
    break;
  case 't'://trimed mean
    trimed_mean_aggregation_optimized(n, f);
    break;
  case 'k':// krum
    krum_aggregation_optimized(n, f, m);
    break;
  case 's'://sar
    sar_aggregation_optimized(n, f, r, k);
    break;
  default:
    LOG(ERROR,__FILE__,__LINE__, "unknown aggregation method");
    break;
  }
  uint8_t encrypt_key[16] = {0x0};
  sgx_aes_gcm_128bit_tag_t tag;
  int dataNodeNum = weights_arr.size();
  for(int i = 0; i < dataNodeNum; i++) {
    sgx_status_t ret = sgx_rijndael128GCM_encrypt((const sgx_aes_gcm_128bit_key_t*)encrypt_key, (uint8_t*)aggregation_result[i], weights_arr[i].size() * sizeof(float), encrypt_aggregation_result[i], aes_gcm_iv, 12, NULL, 0, &tag);
    if (ret != SGX_SUCCESS) {
      LOG(ERROR, __FILE__, __LINE__, "aes gcm encrypt failed");
      return ;
    }
    ocall_save_aggregation_result2(layer_index, encrypt_aggregation_result[i], weights_arr[0].size() * sizeof(float),tag);
  }
}

void ecall_aggregation(int layer_index, void* arguments, size_t size) {
  // prepare the space for aggregation result 
  size_t dataNodeNum = client_data[0]->Size();
  // for aggregation result 
  client_data[client_num] = new dataNodeList();
  // for encrypted aggregation result
  client_data[client_num+1] = new dataNodeList();
  for(int i = 0; i < dataNodeNum; i++) {
    uint32_t data_length = client_data[0]->Get(i)->length_;
    uint8_t *aggregated_data = (uint8_t*) malloc(data_length);
    uint8_t *encrypted_data = (uint8_t*) malloc(data_length);
    client_data[client_num]->Insert(aggregated_data, data_length);
    client_data[client_num+1]->Insert(encrypted_data, data_length);
  }
  sgx_aggregation_arguments* p_args = (sgx_aggregation_arguments*) arguments;
  char a = p_args->a;
  int n = p_args->n;
  int f = p_args->f;
  int m = p_args->m;
  float r = p_args->r;
  int k = p_args->k;
  switch (a)
  {
  case 'a'://average
    average_aggregation(n);
    break;
  case 'm'://median
    median_aggregation_straightforward(n);
    break;
  case 't'://trimed mean
    trimed_mean_aggregation(n, f);
    break;
  case 'k':// krum
    krum_aggregation(n, f, m);
    break;
  case 's'://sar
    sar_aggregation(n, f, r, k);
    break;
  default:
    LOG(ERROR,__FILE__,__LINE__, "unknown aggregation method");
    break;
  }
  uint8_t encrypt_key[16] = {0x0};
  
  sgx_aes_gcm_128bit_tag_t tag;
  for (int i = 0; i < dataNodeNum; i++) {
    sgx_status_t ret = sgx_rijndael128GCM_encrypt((const sgx_aes_gcm_128bit_key_t *)encrypt_key, client_data[client_num]->Get(i)->data_, client_data[client_num]->Get(i)->length_, client_data[client_num+1]->Get(i)->data_, aes_gcm_iv, 12, NULL, 0, &tag);
    if (ret != SGX_SUCCESS) {
      LOG(ERROR, __FILE__, __LINE__, "aes gcm encrypt failed");
      return ;
    }
    ocall_save_aggregation_result2(layer_index, client_data[client_num+1]->Get(i)->data_, client_data[client_num+1]->Get(i)->length_, tag);
  }
  delete client_data[client_num];
  delete client_data[client_num+1];
}


