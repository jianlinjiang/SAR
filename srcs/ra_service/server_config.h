// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include <butil/logging.h>
using namespace std;
using json = nlohmann::json;
namespace sar
{
  class ServerConfig {
  public:
    ServerConfig(const string &config_path);
    bool ParseAndSetConfigs();
    int GetPort() const { return port_; }
    string GetSSLCert() const { return ssl_cert_; } 
    string GetSSLPrivateKey() const { return ssl_private_key_; }
  private:
    json config_;
    int port_;
    string ssl_cert_;
    string ssl_private_key_;
  };
} // namespace ra

#endif