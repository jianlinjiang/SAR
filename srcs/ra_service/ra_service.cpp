// MIT License

// Copyright (c) 2020 jianlinjiang

#include <brpc/server.h>
#include "ra.pb.h"
namespace ra
{
  class RaServiceImpl : public RaService
  {
  public:
    RaServiceImpl() {}
    ~RaServiceImpl() {}
    void RaProcess(google::protobuf::RpcController *cntl_base,
                   const RaRequset *request,
                   RaResponse *response,
                   google::protobuf::Closure *done)
    {
      brpc::ClosureGuard done_guard(done);
      brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);

      response->set_messgae(request->message());
    }
  };

} // namespace ra