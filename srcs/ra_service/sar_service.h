// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef SAR_SERVICE_H
#define SAR_SERVICE_H
#include <brpc/server.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "sar.pb.h"
#include "sar_message.h"
#include "sar_util.h"


namespace sar {
  class SarServiceImpl : public SarService {
  public:
    SarServiceImpl() {}
    virtual ~SarServiceImpl() {}
    virtual void transmitWeights(google::protobuf::RpcController *cntl_base, const SarRequest *request, SarResponse *response, google::protobuf::Closure *done);
    virtual void loadWeights(google::protobuf::RpcController *cntl_base, const loadWeightsRequest *request, SarResponse *response, google::protobuf::Closure *done);

    virtual void startAggregation(google::protobuf::RpcController *cntl_base, const StartAggregationRequest *request, SarResponse *response, google::protobuf::Closure *done); 
  private:
     void transmitError(SarResponse *response);
  };
}

#endif