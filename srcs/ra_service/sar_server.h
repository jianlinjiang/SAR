// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef SAR_SERVER_H
#define SAR_SERVER_H
#include <brpc/server.h>
#include <butil/logging.h>
#include "server_config.h"
namespace sar {
  const static int idle_time_out = -1;
  const static int max_concurrency = 0;
  const static int internal_port = -1;
  class SarServer {
  public:
    SarServer(const ServerConfig&);
    ~SarServer();

    bool Start();
    void Stop();
    bool AddService(google::protobuf::Service *service);
    
  private:

    brpc::ServerOptions options_;
    brpc::Server server_;
    int port_;
  };
}

#endif