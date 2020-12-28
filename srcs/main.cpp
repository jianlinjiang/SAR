// MIT License

// Copyright (c) 2020 jianlinjiang

#include <unistd.h>
#include <iostream>
#include <string>

#include "ra_service/ra_service.h"
#include "ra_service/sar_service.h"
#include "ra_service/server_config.h"
#include "ra_service/sar_server.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "ra_service/enclave_context.h"

using namespace std;

int total_client = 100;

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

  sar::EnclaveContext enclave_context;
  
  sar::ServerConfig server_config("config.json");
  if (!server_config.ParseAndSetConfigs())
  {
    LOG(FATAL) << "parse the config file failed!\n";
  }

  sar::SarServer sar_server(server_config);

  ra::RaServiceImpl ra_service_impl;
  sar::SarServiceImpl sar_service_impl;
  if (!sar_server.AddService(&ra_service_impl))
  {
    LOG(ERROR) << "Failed to add service!\n";
    return 0;
  }
  if (!sar_server.AddService(&sar_service_impl))
  {
    LOG(ERROR) << "Failed to add service!\n";
    return 0;
  }

  if (!sar_server.Start())
  {
    LOG(ERROR) << "Failed to start sar server!\n";
  }

  sar_server.Stop();

  return 0;
}