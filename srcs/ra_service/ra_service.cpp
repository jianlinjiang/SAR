// MIT License

// Copyright (c) 2020 jianlinjiang

#include <brpc/server.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
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
      func_table_[CLIENT_CHALLENGE] = RaServiceImpl::process_client_challenge;
      func_table_[CLIENT_MSG0] = RaServiceImpl::process_client_msg0;
    }
    virtual ~RaServiceImpl() {}

    virtual void Ra(google::protobuf::RpcController *cntl_base,
                   const RaRequest* request,
                   RaResponse* response,
                   google::protobuf::Closure* done)
    {
      LOG(INFO) << "get request from client";
      brpc::ClosureGuard done_guard(done);
      brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
      const char *request_message = request->message().c_str();
      message_type type = message_type(*(int*)request_message);

      auto func_iter = func_table_.find(type);
      if (func_iter == func_table_.end()) {
        LOG(ERROR) << "unknown message type!";
        return ;
      }

      // call the function
      if (!func_iter->second(request_message, response)) {
        LOG(ERROR) << "internal error happen";
        ra_message *response_message = (_ra_message_t*)malloc(sizeof(_ra_message_t));
        response_message->type = INTERNAL_ERROR;
        memset(response_message->uuid, 0x0, UUID_LENGTH);
        response_message->length = 0;
        response->set_message((const char*)response_message, sizeof (_ra_message_t));
      }
    }

  private:
    //map message type to process function
    std::map<message_type, process_message_func> func_table_;

    static bool process_client_challenge(const char *request_message, RaResponse *response)
    {
      // check uuid is empty
      request_message += sizeof(message_type);
      if (!check_arr_is_zero((uint8_t*)request_message, UUID_LENGTH)) {
        LOG(ERROR) << "challenge message uuid isn't nil!";
        return false;
      }

      // the message length should be 0
      request_message += UUID_LENGTH;
      uint32_t length = *(uint32_t*) request_message;
      if (length != 0) {
        LOG(ERROR) << "the challenge message body length isn't 0!";
        return false;
      }

      // init ra context
      sgx_ra_context_t context = INT_MAX;
      sgx_status_t retval = SGX_SUCCESS;
      // generated ra context
      sgx_status_t ret = ecall_enclave_ra_init(global_eid, &retval, false, &context);
      if (retval != SGX_SUCCESS) {
        LOG(ERROR) << "enclave ra init failed";
        return false;
      }

      // generate random uuid for the client
      boost::uuids::random_generator gen;
      boost::uuids::uuid u = gen();
      std::string uuid_s = boost::uuids::to_string(u);
      // save the context for later use
      g_client_context_map.insert(std::make_pair(uuid_s, context));

      // get extened epid group id
      ret = sgx_get_extended_epid_group_id(&g_extended_epid_group_id);
      if (ret != SGX_SUCCESS) {
        LOG(ERROR) << "get extened epid failed" << get_error_message(ret);
        return false;
      }

      // generate a response
      uint32_t challenge_rsp_length = sizeof(_ra_challenge_response_t);
      ra_challenge_response *challenge_rsp = (ra_challenge_response*)malloc(challenge_rsp_length);
      challenge_rsp->extended_group_id = g_extended_epid_group_id;

      // generate ra message response
      ra_message *reponse_message = (ra_message*) malloc(sizeof(_ra_message_t)+challenge_rsp_length);
      reponse_message->type = SERVER_CHALLENGE_RESPONSE;
      memcpy(reponse_message->uuid, uuid_s.c_str(), UUID_LENGTH);
      reponse_message->length = challenge_rsp_length;
      memcpy(reponse_message->body, (uint8_t*) challenge_rsp, challenge_rsp_length);
      
      //set rpc response
      response->set_message((char*) reponse_message, sizeof(_ra_message_t) + challenge_rsp_length);

      free(challenge_rsp);
      free(reponse_message); 
      return true;
    }

    static bool process_client_msg2(const char *request_message, RaResponse* response) {
      ra_message *request_msg = (ra_message*) request_message;
      std::string uuid_s((const char *) request_msg->uuid, UUID_LENGTH);
      sgx_ra_msg2_t *msg2 = (sgx_ra_msg2_t*)request_msg->body;
      // generate msg3 
      int busy_retry_time = 2;
      int ret = 0;
      uint32_t msg3_size = 0;
      // find the context for the client
      auto iter = g_client_context_map.find(uuid_s);
      if (iter == g_client_context_map.end()) {
        LOG(ERROR) << "can't find client's uuid, unknown client";
        return false;
      }
      sgx_ra_context_t &context = iter->second;
      sgx_ra_msg3_t *p_msg3 = NULL;
      do {
        ret = sgx_ra_proc_msg2_ex(&g_selected_key_id, context, global_eid, sgx_ra_proc_msg2_trusted, sgx_ra_get_msg3_trusted, msg2, request_msg->length, &p_msg3, &msg3_size);
      } while( ret == SGX_ERROR_BUSY && busy_retry_time--);
      if (!p_msg3) {
        LOG(ERROR) << "call sgx_ra_proc_msg2_ex failed ";
        return false;
      }
      if (SGX_SUCCESS != (sgx_status_t)ret){
        LOG(ERROR) << "call sgx_ra_proc_msg2_ex failed ret :" << ret;
        LOG(ERROR) << get_error_message((sgx_status_t)ret);
        return false;
      } else {
        LOG(INFO) << "call sgx_ra_proc_msg2_ex success";
      }

      // send msg 3 to client
      uint32_t rsp_size = sizeof(_ra_message_t) + msg3_size;
      ra_message* response_msg = (ra_message*) malloc(rsp_size);
      response_msg->type = SERVER_MSG3;
      response_msg->length = msg3_size;
      memcpy(response_msg->uuid, uuid_s.c_str(), UUID_LENGTH);
      memcpy(response_msg->body, (const uint8_t*)p_msg3, msg3_size);

      response->set_message((const char*) response_msg, rsp_size);

      if (p_msg3 != NULL) free(p_msg3);
      free(response_msg);
      return true;
    }

    static bool process_client_msg0(const char *request_message, RaResponse *response) {
      ra_message *request_msg = (ra_message*) request_message; 
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
  
      // msg0 is the support key id list 
      sgx_status_t ret = sgx_select_att_key_id(request_msg->body, request_msg->length, &g_selected_key_id);
      if (ret != SGX_SUCCESS){
        LOG(ERROR) << "select key id failed" << get_error_message(ret);
        return false;
      }

      // generate MSG1 
      auto iter = g_client_context_map.find(uuid_s);
      if (iter == g_client_context_map.end()) {
        LOG(ERROR) << "can't find client's uuid, unknown client";
        return false;
      }
      sgx_ra_context_t &context = iter->second;

      uint32_t msg1_size = sizeof(sgx_ra_msg1_t);
      ra_message* response_msg = (ra_message*) malloc(sizeof(_ra_message_t) + msg1_size);
      
      sgx_ra_msg1_t msg1;
      ret = SGX_SUCCESS;
      int busy_retry_time = 3;
      // ecall to get msg1 inside enclave
      do 
      {
        sgx_ra_get_msg1_ex(&g_selected_key_id, context, global_eid, sgx_ra_get_ga, &msg1);
      } while(SGX_ERROR_BUSY == ret && busy_retry_time--);

      if (ret != SGX_SUCCESS) {
        LOG(ERROR) << "sgx ra get msg1 failed!";
        return false;
      }
      response_msg->type = SERVER_MSG1;
      response_msg->length = msg1_size;
      memcpy(response_msg->uuid, uuid_s.c_str(), UUID_LENGTH);
      memcpy(response_msg->body, (const uint8_t*) &msg1, msg1_size);
      
      // set response
      response->set_message((const char*) response_msg, sizeof(_ra_message_t) + msg1_size);
      
      free(response_msg); 
      return true;
    }
  };

} // namespace ra