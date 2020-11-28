// MIT License

// Copyright (c) 2020 jianlinjiang

#include <brpc/server.h>
#include <map>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "ra.pb.h"
#include "ra_message.h"
#include "sar_util.h"
typedef bool (*process_message_func)(const char *, ra::RaResponse *);
namespace ra
{

  class RaServiceImpl : public RaService
  {
  public:
    RaServiceImpl()
    {
      // register message process function
      func_table_[CLIENT_CHALLENGE] = process_client_challenge;
    }
    ~RaServiceImpl() {}
    void RaProcess(google::protobuf::RpcController *cntl_base,
                   const RaRequset *request,
                   RaResponse *response,
                   google::protobuf::Closure *done)
    {
      brpc::ClosureGuard done_guard(done);

      const char *message = request->message().c_str();
      message_type type = message_type(*(int*)message);

      auto func_iter = func_table_.find(type);
      if (func_iter == func_table_.end()) {
        LOG(ERROR) << "unknown message type!";
        return ;
      }

      // calll the function
      func_iter->second(message, response);



      response->set_message(request->message());
    }

  private:
    //map message type to process function
    std::map<message_type, process_message_func> func_table_;

    static bool process_client_challenge(const char *message, RaResponse *response)
    {
      // check uuid is empty
      message += sizeof(message_type);
      if (!check_arr_is_zero((uint8_t*)message, UUID_LENGTH)) {
        LOG(ERROR) << "challenge message uuid isn't nil!";
        return false;
      }

      // the message length should be 0
      message += UUID_LENGTH;
      uint32_t length = *(uint32_t*) message;
      if (length != 0) {
        LOG(ERROR) << "the message body length isn't 0!";
        return false;
      }

      // generate random uuid for the client
      boost::uuids::random_generator gen;
      boost::uuids::uuid u = gen();
      uint8_t p[UUID_LENGTH];
      memcpy(p, u.data, UUID_LENGTH);

    }
  };

} // namespace ra