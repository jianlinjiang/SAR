// MIT License

// Copyright (c) 2020 jianlinjiang

#include <brpc/server.h>
#include "ra.pb.h"
#include "ra_message.h"
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
      
      const char *messgae = request->message().c_str();

      response->set_messgae(request->message());
    }
  private:
    bool process_message(const char* messgae) {
      message_type type = message_type(*(int*)messgae);
      if (type == CLIENT_CHALLENGE) {
        // client first challenge messgae
        
      }
    }
  };
  

} // namespace ra