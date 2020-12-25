// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef RA_SERVICE_H
#define RA_SERVICE_H

#include <brpc/server.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "ra.pb.h"
#include "ra_message.h"
#include "sar_util.h"

typedef bool (*process_message_func)(const char *, ra::RaResponse *);

namespace ra {
  class RaServiceImpl : public RaService {
public:
  RaServiceImpl() {};
  virtual ~RaServiceImpl() {};
  virtual void Challenge(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done);
  virtual void Msg0Process(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done);
  virtual void Msg2Process(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done);
  virtual void Msg4Process(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done);

private:
  static void removeRaContext(std::string uuid_s);
  bool sendErrorMessageToClient(RaResponse *response);
  };
};


#endif