// MIT License

// Copyright (c) 2020 jianlinjiang

#include "sar_server.h"

sar::SarServer::SarServer(const ServerConfig &config) {
  port_ = config.get_port();

  options_.mutable_ssl_options()->default_cert.certificate = config.get_ssl_cert();
  options_.mutable_ssl_options()->default_cert.private_key = config.get_ssl_private_key();

  options_.idle_timeout_sec = idle_time_out;
  options_.max_concurrency = max_concurrency;
  options_.internal_port = -1;
}

bool sar::SarServer::AddService(google::protobuf::Service *service) {
  if (server_.AddService(service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    return false;
  }
  return true;
}

bool sar::SarServer::Start() {
  if(server_.Start(port_, &options_) !=0) {
    return false;
  }
  return true;
}

sar::SarServer::~SarServer(){
  server_.RunUntilAskedToQuit();
}