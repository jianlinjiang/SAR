// MIT License

// Copyright (c) 2020 jianlinjiang

#include <unistd.h>
#include <iostream>
#include <string>

#include "ra_service/ra_service.cpp"
#include "ra_service/server_config.h"
#include "ra_service/sar_server.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "ra_service/enclave_context.h"

using namespace std;
int main(int argc, char *argv[])
{
  // logging setting
  int pid = (int)getpid();
  string log_file = "logs/SAR_SERVER." + to_string(pid);
  logging::LoggingSettings logSetting;
  // set logging dest
  logSetting.logging_dest = logging::LOG_TO_ALL;
  logSetting.log_file = log_file.c_str();
  logSetting.lock_log = logging::LOCK_LOG_FILE;
  logSetting.delete_old = logging::DELETE_OLD_LOG_FILE;

  InitLogging(logSetting);

  // sar::ServerConfig server_config("config.json");
  // if (!server_config.ParseAndSetConfigs()) {
  //   LOG(FATAL) << "parse the config file failed!\n";
  // }

  // sar::SarServer sar_server(server_config);

  // // brpc::Server server;
  // ra::RaServiceImpl ra_service_impl;

  // if (!sar_server.AddService(&ra_service_impl)) {
  //   LOG(ERROR) << "Failed to add service!\n";
  // }

  // if (!sar_server.Start()) {
  //   LOG(ERROR) << "Failed to start sar server!\n";
  // }
  boost::uuids::random_generator gen;
  boost::uuids::uuid u = gen();
  uint8_t p[UUID_LENGTH];
  memcpy(p, u.data, UUID_LENGTH);
  for(int i = 0; i < UUID_LENGTH; i++){
    printf("0x%02x ",p[i]);
  }
  printf("\n");
  // sar::EnclaveContext enclave_context;

  // if(server.AddService(&ra_service_impl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
  //   LOG(ERROR) << "Fail to add service";
  //   return -1;
  // }

  // brpc::ServerOptions options;
  // options.mutable_ssl_options()->default_cert.certificate = "cert.pem";
  // options.mutable_ssl_options()->default_cert.private_key = "key.pem";
  // // options.idle_timeout_sec = FLAGS_idle_timeout_s;
  // // options.max_concurrency = FLAGS_max_concurrency;
  // options.internal_port = -1;

  // if(server.Start(8002, &options) != 0) {
  //   LOG(ERROR) << "Fail to start SARServer";
  // }

  // server.RunUntilAskedToQuit();

  return 0;
}