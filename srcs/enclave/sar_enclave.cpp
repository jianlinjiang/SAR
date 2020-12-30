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
      if (p->data_) free(p->data_);
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
      heap = std::vector<std::pair<double, int>>(1, std::make_pair(0.0, 0));
    }
    void Push(double score, int index) {
      if (heap.size() <= k) {
        heap.push_back(std::make_pair(score, index));
        up(heap.size()-1);
      } else {
        if (heap[1].first > score) {
          heap[1].first = score;
          heap[1].second = index;
          down(1);
        }
      }
    }
    int GetIndex(int i) { //start from 1
      return heap[i+1].second;
    }
  private:
    // index start from 1
    void down(int u) {
      int t = u;
      if (u*2 <= heap.size()-1 && heap[u*2].first > heap[t].first) t = u*2;
      if (u*2+1 <= heap.size()-1 && heap[u*2+1].first > heap[t].first) t = u*2+1;
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
int max_num = 1000;
dataNodeList *client_data[1000] = {NULL};
void free_load_weight_context()
{
  for (int i = 0; i < max_num; i++)
  {
    if (client_data[i])
      delete client_data[i];
    client_data[i] = nullptr;
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

// client 0 as the result
void average_aggregation(int n) {
  size_t dataNodeNum = client_data[0]->Size();
  for (int k = 0; k < dataNodeNum; k ++) {
    uint32_t length = client_data[0]->Get(k)->length_;
    size_t weights_num = length / sizeof(float); 
    float* res = (float*) client_data[0]->Get(k)->data_;
    for (int i = 1; i < n; i ++) {
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
    float* result_data = (float*) client_data[0]->Get(k)->data_;
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

void median_aggregation_straightforward(int n) {
  size_t dataNodeNum = client_data[0]->Size();
  for (int k = 0; k < dataNodeNum; k++) {
    uint32_t length = client_data[0]->Get(k)->length_;
    size_t weights_num = length / sizeof(float);
    std::vector<float> weights(n, 0);
    float *result_data = (float*)client_data[0]->Get(k)->data_;
    for(int i = 0; i < weights_num; i++) { 
      for(int j = 0; j < n; j++) {
        float* weight_data = ((float*)client_data[j]->Get(k)->data_) + i;
        weights[j] = *weight_data;
      }
      float result = 0;
      if (n % 2 == 0) {
        // even
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
        result = weights[weights.size()/2];
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2-1, weights.end());
        result += weights[weights.size()/2-1];
        result /= 2;
      } else {
        // odd 
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
        result = weights[weights.size()/2];
      }
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


void sar_aggregation(int n, int f, float r, int k) {
  size_t dataNodeNum = client_data[0]->Size();
  std::vector<std::vector<double>> distance(n, std::vector<double>(n, 0));
  for (int i = 0; i < n; i++) {
    for (int j = i+1; j < n; j++) {
      double d = 0;
      for (int x = 0; x < dataNodeNum; x++) {
        uint32_t weights_num = client_data[0]->Get(x)->length_ / sizeof(float);
        uint32_t select_num = (uint32_t)weights_num * r;
        std::vector<int> selected = pick(weights_num, select_num);
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
  // calculate the median of k smallest scores weights
  pair_heap heap(k);
  for (int i = 0; i < n; i++) {
    heap.Push(scores[i], i);
  }
  // calculate the median of the top k weights
  for (int x = 0; x < dataNodeNum; x++) {
    size_t weights_num = client_data[0]->Get(x)->length_/ sizeof(float);
    std::vector<float> weights(k, 0);
    // the data is stored in client_data[0]
    float* result_data = (float*)client_data[0]->Get(x)->data_;
    for(int i = 0; i < weights_num; i++) {
      for(int j = 0; j < k; j++) {
        int client_index = heap.GetIndex(j);
        float* weight_data = ((float*)client_data[client_index]->Get(x)->data_) + i;
        weights[j] = *weight_data;
      }
      float result = 0;
      if (k % 2 == 0) {
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
        result = weights[weights.size()/2];
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2-1, weights.end());
        result += weights[weights.size()/2-1];
        result /= 2;
      } else {
        std::nth_element(weights.begin(), weights.begin()+weights.size()/2, weights.end());
        result = weights[weights.size()/2];
      }
      result_data[i] = result;
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
  // swap client0 and client i
  std::swap(client_data[0], client_data[index]);
}

void ecall_aggregation(void* arguments, size_t size) {
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
  LOG(INFO, __FILE__, __LINE__, "aggregation finished");
  uint8_t encrypt_key[16] = {0x0};
  size_t dataNodeNum = client_data[0]->Size();
  sgx_aes_gcm_128bit_tag_t tag;
  // client_data[1] as the encrypt result
  for (int i = 0; i < dataNodeNum; i++) {
    sgx_status_t ret = sgx_rijndael128GCM_encrypt((const sgx_aes_gcm_128bit_key_t *)encrypt_key, client_data[0]->Get(i)->data_, client_data[0]->Get(i)->length_, client_data[1]->Get(i)->data_, aes_gcm_iv, 12, NULL, 0, &tag);
    if (ret != SGX_SUCCESS) {
      LOG(ERROR, __FILE__, __LINE__, "aes gcm encrypt failed");
      return ;
    }
    ocall_save_aggregation_result(i, client_data[1]->Get(i)->data_, client_data[1]->Get(i)->length_, tag);
  }
  
}



