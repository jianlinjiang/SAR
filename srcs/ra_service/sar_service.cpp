// MIT License

// Copyright (c) 2020 jianlinjiang

#include "sar_service.h"

namespace sar
{
  void SarServiceImpl::transmitError(SarResponse *response)
  {
    transmit_response response_msg;
    response_msg.type = INTERNAL_ERROR;
    response->set_message((const char *)&response_msg);
  }

  void SarServiceImpl::transmitWeights(google::protobuf::RpcController *cntl_base, const SarRequest *request, SarResponse *response, google::protobuf::Closure *done)
  {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
    transmit_message *request_msg = (transmit_message *)request->message().c_str();
    transmit_response response_msg;
    std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
    bool ret = true;
    // find the uuid in g_client_directory_map
    auto iter = g_client_directory_map.end();
    {
      std::unique_lock<std::mutex> lk(g_context_map_mutex);
      iter = g_client_directory_map.find(uuid_s);
    }
    int directory = -1;
    std::string client_directory = "weights/";
    int fd = -1;
    
    // filename "weights/1/layer1"
    if (iter == g_client_directory_map.end())
    {
      // doesn't exit, means first time to upload
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
      // first time the directory doesn't exist
      // assert(access(client_directory.c_str(), 0) != 0);
      mkdir(client_directory.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    else
    {
      // the directory already exists
      directory = iter->second;
      client_directory += std::to_string(directory);
    }

    uint32_t layer = request_msg->layer_num;
    std::string filename = client_directory + "/" + std::to_string(layer);
    size_t write_bytes = 0;
    if (request_msg->flag == START)
    {
      fd = open(filename.c_str(), O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      if (fd == -1)
      {
        LOG(ERROR) << "open weights file failed, filename " << filename;
        ret = false;
        goto cleanup;
      }
      if (request_msg->length == TRANSMIT_SIZE)
      {
        // file size is smaller than transmit_size, the file is transmit in one time
        std::unique_lock<std::mutex> lk(g_context_map_mutex);
        g_client_file_map.insert(std::make_pair(filename, fd));
      }
    }
    if (fd == -1)
    {
      std::unique_lock<std::mutex> lk(g_context_map_mutex);
      auto fd_iter = g_client_file_map.find(filename);
      assert(fd_iter != g_client_file_map.end());
      fd = g_client_file_map[filename];
    }
    // the format in the file 
    // uint32_t length 
    // uint8_t[16] tag
    // ciphertext
    write_bytes = write(fd, &request_msg->length, sizeof(uint32_t));
    if (write_bytes != sizeof(uint32_t)) {
      ret = false;
      LOG(ERROR) << "write length to file failed";
      goto cleanup;
    }
    write_bytes = write(fd, request_msg->tag, sizeof(request_msg->tag));
    if (write_bytes != sizeof(request_msg->tag)) {
      ret = false;
      LOG(ERROR) << "write tag to file failed";
      goto cleanup;
    }
    write_bytes = write(fd, request_msg->body, request_msg->length);
    if (write_bytes != request_msg->length)
    {
      ret = false;
      LOG(ERROR) << "write ciphertext to file failed";
      goto cleanup;
    }
    if (request_msg->flag == END || request_msg->length < TRANSMIT_SIZE)
    {
      if (close(fd) != 0)
      {
        ret = false;
        LOG(ERROR) << "close file failed";
      }
      if (request_msg->flag == END)
      {
        g_client_file_map.erase(filename);
        fd = -1;
      }
    }
    response_msg.type = SERVER_TRANSMIT_RESPONSE;
    response->set_message((const char *)&response_msg, sizeof(response_msg));
  cleanup:
    if (!ret)
    {
      transmitError(response);
      if (fd != -1)
      {
        close(fd);
        auto fd_iter = g_client_file_map.find(filename);
        if (fd_iter != g_client_file_map.end())
        {
          g_client_file_map.erase(fd_iter);
        }
      }
    }
  }
} // namespace sar