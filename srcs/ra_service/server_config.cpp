// MIT License

// Copyright (c) 2020 jianlinjiang

#include "server_config.h"

sar::ServerConfig::ServerConfig(const string& config_path) {
  ifstream i(config_path.c_str());
  if (!i.is_open()) {
    LOG(ERROR) << "can't open the config file" << config_path;
  }
  i >> config_;
}

bool sar::ServerConfig::ParseAndSetConfigs() {
  if (config_.contains("Server_port")) {
    port_ = config_["Server_port"];
  } else {
    LOG(FATAL) << "The config file doesn't contain the key Server_port. \n";
    return false;
  }

  if (config_.contains("Server_ssl_cert")) {
    ssl_cert_ = config_["Server_ssl_cert"];
  } else {
    LOG(FATAL) << "The config file doesn't contain the key Server_ssl_cert.\n";
    return false;
  }

  if (config_.contains("Server_ssl_private_key")) {
    ssl_private_key_ = config_["Server_ssl_private_key"];
  } else {
    LOG(FATAL) << "The config file doesn't contain the key Server_ssl_private_key.\n";
    return false;
  }

  return true;
}