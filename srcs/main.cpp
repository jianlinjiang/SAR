// MIT License

// Copyright (c) 2020 jianlinjiang

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include "ra_service/server.cpp"

DEFINE_int32(port, 8002, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
                                 "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
                              "(waiting for client to close connection before server stops)");
DEFINE_int32(max_concurrency, 0, "Limit of request processing in parallel");

DEFINE_int32(internal_port, -1, "Only allow builtin services at this port");

DEFINE_bool(h, false, "print help infomation");
int main(int argc, char *argv[])
{
  std::string help_str = "dummy help infomation";
  google::SetUsageMessage(help_str);

  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_h)
  {
    fprintf(stderr, "%s\n%s\n%s", help_str.c_str(), help_str.c_str(), help_str.c_str());
    return 0;
  }

  brpc::Server server;
  ra::RaServiceImpl ra_service_impl;

  if(server.AddService(&ra_service_impl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "Fail to add service";
    return -1;
  }

  brpc::ServerOptions options;
  options.mutable_ssl_options()->default_cert.certificate = "cert.pem";
  options.mutable_ssl_options()->default_cert.private_key = "key.pem";
  options.idle_timeout_sec = FLAGS_idle_timeout_s;
  options.max_concurrency = FLAGS_max_concurrency;
  options.internal_port = FLAGS_internal_port;

  if(server.Start(FLAGS_port, &options) != 0) {
    LOG(ERROR) << "Fail to start SARServer";
  }

  server.RunUntilAskedToQuit();

  return 0;
}