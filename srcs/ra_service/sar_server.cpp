// MIT License

// Copyright (c) 2020 jianlinjiang

#include "sar_server.h"

sar::SarServer::SarServer(const ServerConfig &config) {
  port_ = config.GetPort();

  options_.mutable_ssl_options()->default_cert.certificate = config.GetSSLCert();
  options_.mutable_ssl_options()->default_cert.private_key = config.GetSSLPrivateKey();

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

void sar::SarServer::Stop() {
  server_.RunUntilAskedToQuit();
}

sar::SarServer::~SarServer(){
}

