// MIT License

// Copyright (c) 2020 jianlinjiang

#include "sar_service.h"
extern int errno;
namespace sar
{
  void SarServiceImpl::transmitError(SarResponse *response)
  {
    transmit_response response_msg;
    response_msg.type = INTERNAL_ERROR;
    response->set_message((const char *)&response_msg, sizeof(transmit_response));
  }

  void SarServiceImpl::loadWeights(google::protobuf::RpcController *cntl_base, const loadWeightsRequest *request, SarResponse *response, google::protobuf::Closure *done)
  {
    // load data into enclave
    brpc::ClosureGuard done_guard(done);
    brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
    int layers_num = request->layers_num();
    int clients_num = request->clients_num();

    std::string weights_directory = "weights/";
    uint8_t buff[TRANSMIT_SIZE];
    uint8_t tag[16];
    uint32_t length = 0;
    size_t read_bytes = 0;
    int fd = -1;
    bool ret = true;
    sgx_ra_context_t *context_arr = (sgx_ra_context_t *)malloc(sizeof(sgx_ra_context_t) * clients_num);
    int *index_arr = (int *)malloc(sizeof(int) * clients_num);
    int i = 0;
    //////////////////////////////////////////
    // argument
    char method[5] = {'a', 'm', 't', 'k', 's'};
    aggregation_arguments arguments;
    arguments.a = method[request->aggregation_method()];
    arguments.n = clients_num;
    arguments.f = request->f();
    arguments.m = request->m();
    arguments.r = request->r();
    arguments.k = request->k();
    //////////////////////////////////////////
    transmit_response response_msg;
    for (auto iter = g_client_context_map.begin(); iter != g_client_context_map.end(); iter++, i++)
    {
      auto index_iter = g_client_directory_map.find(iter->first);
      assert(index_iter != g_client_directory_map.end());
      context_arr[i] = iter->second;
      index_arr[i] = index_iter->second;
    }
    sgx_status_t retval = SGX_SUCCESS;
    sgx_status_t sgxret = ecall_create_weights_load_context(global_eid, &retval, context_arr, index_arr, sizeof(sgx_ra_context_t) * clients_num);
    if (sgxret != SGX_SUCCESS || retval != SGX_SUCCESS)
    {
      LOG(ERROR) << get_error_message(retval);
      ret = false;
      goto cleanup;
    }

    // load each layer into the enclave
    for (int i = 0; i < layers_num; i++)
    {
      // aggregate each layer
      for (int j = 0; j < clients_num; j++)
      {
        std::string filename = weights_directory + std::to_string(j) + "/" + std::to_string(i);
        fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1)
        {
          LOG(ERROR) << "can't open file " << filename;
          ret = false;
          goto cleanup;
        }
        while (true)
        {
          // uint32_t length 4 bytes
          // tag
          // encrypted bytes
          read_bytes = read(fd, &length, 4);
          if (read_bytes == 0)
            break;
          if (read_bytes != 4)
          {
            LOG(ERROR) << "read length failed from the file " << filename;
            ret = false;
            goto cleanup;
          }
          read_bytes = read(fd, tag, 16);
          if (read_bytes != 16)
          {
            LOG(ERROR) << "read tag failed from the file" << filename;
            ret = false;
            goto cleanup;
          }
          read_bytes = read(fd, buff, length);
          if (read_bytes != length)
          {
            LOG(ERROR) << "read encrypted data from the file" << filename;
            ret = false;
            goto cleanup;
          }
          // load data into the enclave
          sgxret = ecall_load_weights(global_eid, &retval, buff, length, tag, j);
          if (sgxret != SGX_SUCCESS || retval != SGX_SUCCESS)
          {
            LOG(ERROR) << "load weight into enclave failed";
            ret = false;
            goto cleanup;
          }
        };
        if (close(fd) != 0)
        {
          ret = false;
          LOG(ERROR) << "close file failed, errno " << errno;
          goto cleanup;
        }
      }
      // start aggregation
      sgxret = ecall_aggregation(global_eid, i, &arguments, sizeof(arguments));
      if (sgxret != SGX_SUCCESS)
      {
        LOG(ERROR) << "aggregation failed";
      }
      else
      {
        // LOG(INFO) << "aggregation finished ";
      }
      sgxret = ecall_free_load_weight_context(global_eid);
      if (sgxret != SGX_SUCCESS)
      {
        LOG(ERROR) << "free context failed";
      }
    }
  cleanup:
    if (context_arr)
      free(context_arr);
    if (index_arr)
      free(index_arr);
    if (!ret)
      transmitError(response);
    else
    {
      transmit_response response_msg;
      response_msg.type = SERVER_LOAD_RESPONSE;
      response->set_message((const char *)&response_msg, sizeof(transmit_response));
    }
  }

  void SarServiceImpl::loadWeightsOptimized(google::protobuf::RpcController *cntl_base, const loadWeightsRequest *request, SarResponse *response, google::protobuf::Closure *done)
  {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
    int layers_num = request->layers_num();
    int clients_num = request->clients_num();

    std::string weights_directory = "weights/";
    uint8_t buff[TRANSMIT_SIZE];
    uint8_t tag[16];
    uint32_t length = 0;
    size_t read_bytes = 0;
    int fd = -1;
    bool ret = true;
    sgx_ra_context_t *context_arr = (sgx_ra_context_t *)malloc(sizeof(sgx_ra_context_t) * clients_num);
    int *index_arr = (int *)malloc(sizeof(int) * clients_num);
    int i = 0;
    //////////////////////////////////////////
    // argument
    char method[5] = {'a', 'm', 't', 'k', 's'};
    aggregation_arguments arguments;
    arguments.a = method[request->aggregation_method()];
    arguments.n = clients_num;
    arguments.f = request->f();
    arguments.m = request->m();
    arguments.r = request->r();
    arguments.k = request->k();
    //////////////////////////////////////////
    transmit_response response_msg;
    for (auto iter = g_client_context_map.begin(); iter != g_client_context_map.end(); iter++, i++)
    {
      auto index_iter = g_client_directory_map.find(iter->first);
      assert(index_iter != g_client_directory_map.end());
      context_arr[i] = iter->second;
      index_arr[i] = index_iter->second;
    }
    sgx_status_t retval = SGX_SUCCESS;
    sgx_status_t sgxret = ecall_create_weights_load_context(global_eid, &retval, context_arr, index_arr, sizeof(sgx_ra_context_t) * clients_num);
    if (sgxret != SGX_SUCCESS || retval != SGX_SUCCESS)
    {
      LOG(ERROR) << get_error_message(retval);
      ret = false;
      goto cleanup;
    }

    // load each layer into the enclave
    for (int i = 0; i < layers_num; i++)
    {
      // aggregate each layer
      for (int j = 0; j < clients_num; j++)
      {
        std::string filename = weights_directory + std::to_string(j) + "/" + std::to_string(i);
        fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1)
        {
          LOG(ERROR) << "can't open file " << filename;
          ret = false;
          goto cleanup;
        }
        int node_index = 0;
        while (true)
        {
          // uint32_t length 4 bytes
          // tag
          // encrypted bytes
          read_bytes = read(fd, &length, 4);
          if (read_bytes == 0)
            break;
          if (read_bytes != 4)
          {
            LOG(ERROR) << "read length failed from the file " << filename;
            ret = false;
            goto cleanup;
          }
          read_bytes = read(fd, tag, 16);
          if (read_bytes != 16)
          {
            LOG(ERROR) << "read tag failed from the file" << filename;
            ret = false;
            goto cleanup;
          }
          read_bytes = read(fd, buff, length);
          if (read_bytes != length)
          {
            LOG(ERROR) << "read encrypted data from the file" << filename;
            ret = false;
            goto cleanup;
          }
          // load data into the enclave
          sgxret = ecall_load_weights_optimized(global_eid, &retval, buff, length, tag, j, node_index);
          if (sgxret != SGX_SUCCESS || retval != SGX_SUCCESS)
          {
            LOG(ERROR) << "load weight into enclave failed";
            ret = false;
            goto cleanup;
          }
          node_index++;
        }
        if (close(fd) != 0)
        {
          ret = false;
          LOG(ERROR) << "close file failed, errno " << errno;
          goto cleanup;
        }
      }
      // start aggregation
      sgxret = ecall_aggregation_optimized(global_eid, i, &arguments, sizeof(arguments));
      if (sgxret != SGX_SUCCESS)
      {
        LOG(ERROR) << "aggregation failed";
      }
      else
      {
        // LOG(INFO) << "aggregation finished ";
      }
      sgxret = ecall_free_load_weight_context_optimized(global_eid);
      if (sgxret != SGX_SUCCESS)
      {
        LOG(ERROR) << "free context failed";
      }
    }
  cleanup:
    if (context_arr)
      free(context_arr);
    if (index_arr)
      free(index_arr);
    if (!ret)
      transmitError(response);
    else
    {
      transmit_response response_msg;
      response_msg.type = SERVER_LOAD_RESPONSE;
      response->set_message((const char *)&response_msg, sizeof(transmit_response));
    }
  }

  void SarServiceImpl::resigsterTransmitWeights(google::protobuf::RpcController *cntl_base, const SarRequest *request, SarResponse *response, google::protobuf::Closure *done)
  {
    // create the weight directory
    brpc::ClosureGuard done_guard(done);
    brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
    transmit_message *request_msg = (transmit_message *)request->message().c_str();
    transmit_response response_msg;
    std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
    bool ret = true;
    int directory = -1;
    std::string client_directory = "weights/";
    directory = client_index.load();
    while (!client_index.compare_exchange_strong(directory, client_index + 1))
    {
      // if index == directory, then index = index + 1
      // else directory == index + 1, continue
    }
    {
      std::unique_lock<std::mutex> lk(g_context_map_mutex);
      g_client_directory_map.insert(std::make_pair(uuid_s, directory));
    }
    client_directory += std::to_string(directory);
    mkdir(client_directory.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    response_msg.type = SERVER_TRANSMIT_RESPONSE;
    response->set_message((const char *)&response_msg, sizeof(response_msg));
  }

  void SarServiceImpl::transmitWeights(google::protobuf::RpcController *cntl_base, const SarRequest *request, SarResponse *response, google::protobuf::Closure *done)
  {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
    transmit_message *request_msg = (transmit_message *)request->message().c_str();
    transmit_response response_msg;
    std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
    bool ret = true;
    // find the directory in g_client_directory_map
    auto iter = g_client_directory_map.end();
    {
      std::unique_lock<std::mutex> lk(g_context_map_mutex);
      iter = g_client_directory_map.find(uuid_s);
    }
    int directory = -1;
    std::string client_directory = "weights/";
    int fd = -1;
    assert(iter != g_client_directory_map.end());
    directory = iter->second;
    client_directory += std::to_string(directory);
    uint32_t layer = request_msg->layer_num;
    std::string filename = client_directory + "/" + std::to_string(layer);
    size_t write_bytes = 0;
    if (request_msg->flag == START)
    {
      int try_times = 5;
      while (try_times-- && fd == -1)
      {
        fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        sleep(0.1);
      }
      if (fd == -1)
      {
        LOG(ERROR) << "open weights file failed, filename " << filename << "errno " << errno;
        ret = false;
        goto cleanup;
      }
      {
        std::unique_lock<std::mutex> lk(g_context_map_mutex);
        g_client_file_map.insert(std::make_pair(filename, fd));
      }
    } else {
      auto fd_iter = g_client_file_map.find(filename);
      assert(fd_iter != g_client_file_map.end());
      fd = fd_iter->second;
    }
    // the format in the file
    // uint32_t length
    // uint8_t[16] tag
    // ciphertext
    if (request_msg->flag != END)
    {
      write_bytes = write(fd, &request_msg->length, sizeof(uint32_t));
      if (write_bytes != sizeof(uint32_t))
      {
        ret = false;
        LOG(ERROR) << "write length to file failed, errno " << errno << " "<< filename;
        goto cleanup;
      }
      write_bytes = write(fd, request_msg->tag, sizeof(request_msg->tag));
      if (write_bytes != sizeof(request_msg->tag))
      {
        ret = false;
        LOG(ERROR) << "write tag to file failed, errno" << errno<< " "<< filename;
        goto cleanup;
      }
      write_bytes = write(fd, request_msg->body, request_msg->length);
      if (write_bytes != request_msg->length)
      {
        ret = false;
        LOG(ERROR) << "write ciphertext to file failed, errno " << errno<< " "<< filename;
        goto cleanup;
      }
    }
    else
    {
      if (close(fd) != 0)
      {
        ret = false;
        LOG(ERROR) << "close file failed, errno " << errno;
      }
      {
        std::unique_lock<std::mutex> lk(g_context_map_mutex);
        g_client_file_map.erase(filename);
      }
      fd = -1;
    }
    response_msg.type = SERVER_TRANSMIT_RESPONSE;
    response->set_message((const char *)&response_msg, sizeof(response_msg));
  cleanup:
    if (!ret)
    {
      transmitError(response);
      if (fd != -1)
      {
        if (close(fd) != 0)
        {
          LOG(ERROR) << "close file failed, errno " << errno;
        }
        {
          std::unique_lock<std::mutex> lk(g_context_map_mutex);
          auto fd_iter = g_client_file_map.find(filename);
          if (fd_iter != g_client_file_map.end())
          {
            g_client_file_map.erase(fd_iter);
          }
        }
      }
    }
  }
} // namespace sar