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
      func_table_[CLIENT_CLOSE_MSG] = RaServiceImpl::process_client_close;
    }
    virtual ~RaServiceImpl() {}

    virtual void RemoteAttestation(google::protobuf::RpcController *cntl_base,
                                   const RaRequest *request,
                                   RaResponse *response,
                                   google::protobuf::Closure *done)
    {
      LOG(INFO) << "get request from client";
      brpc::ClosureGuard done_guard(done);
      brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
      const char *request_message = request->message().c_str();
      message_type type = message_type(*(int *)request_message);

      auto func_iter = func_table_.find(type);
      if (func_iter == func_table_.end())
      {
        LOG(ERROR) << "unknown message type!";
        return;
      }

      // call the function
      if (!func_iter->second(request_message, response))
      {
        LOG(ERROR) << "internal error happen";
        ra_message *response_message = (_ra_message_t *)malloc(sizeof(_ra_message_t));
        response_message->type = INTERNAL_ERROR;
        memset(response_message->uuid, 0x0, UUID_LENGTH);
        response_message->length = 0;
        response->set_message((const char *)response_message, sizeof(_ra_message_t));
      }
    }

  private:
    //map message type to process function
    std::map<message_type, process_message_func> func_table_;

    static bool process_client_challenge(const char *request_message, RaResponse *response)
    {
      ra_message *request_msg = (ra_message*) request_message;
      std::string uuid_s((const char*)request_msg->uuid, UUID_LENGTH);
      bool ret = true;
      // init ra context
      sgx_ra_context_t context = INT_MAX;
      sgx_status_t sgxretval = SGX_SUCCESS;

      boost::uuids::random_generator gen;
      boost::uuids::uuid u = gen();

      uint32_t challenge_rsp_length = 0;
      ra_challenge_response *challenge_rsp = NULL;

      ra_message *response_message = NULL;
      // generated ra context
      sgx_status_t sgxret = ecall_enclave_ra_init(global_eid, &sgxretval, false, &context);
      if (sgxretval != SGX_SUCCESS)
      {
        LOG(ERROR) << "enclave ra init failed";
        ret = false;
        goto cleanup;
      }

      // generate random uuid for the client
      uuid_s = boost::uuids::to_string(u);
      // save the context for later use
      g_client_context_map.insert(std::make_pair(uuid_s, context));

      // get extened epid group id
      sgxret = sgx_get_extended_epid_group_id(&g_extended_epid_group_id);
      if (ret != SGX_SUCCESS)
      {
        LOG(ERROR) << "get extened epid failed" << get_error_message(sgxret);
        ret = false;
        goto cleanup;
      }

      // generate a response
      challenge_rsp_length = sizeof(_ra_challenge_response_t);
      challenge_rsp = (ra_challenge_response *)malloc(challenge_rsp_length);
      challenge_rsp->extended_group_id = g_extended_epid_group_id;
      // generate ra message response
      response_message = (ra_message *)malloc(sizeof(_ra_message_t) + challenge_rsp_length);
      response_message->type = SERVER_CHALLENGE_RESPONSE;
      memcpy(response_message->uuid, uuid_s.c_str(), UUID_LENGTH);
      response_message->length = challenge_rsp_length;
      memcpy(response_message->body, (uint8_t *)challenge_rsp, challenge_rsp_length);

      //set rpc response
      response->set_message((char *)response_message, sizeof(_ra_message_t) + challenge_rsp_length);

    cleanup:
      if (!ret)
      {
        sendErrorMessageToClient(response);
        // if the ra context exists erase the context from the map
        removeRaContext(uuid_s);
      }
      if (challenge_rsp) free(challenge_rsp);
      if (response_message) free(response_message);
      return ret;
    }

    static void removeRaContext(std::string uuid_s)
    {
      auto iter = g_client_context_map.find(uuid_s);
      if (iter != g_client_context_map.end())
      {
        sgx_ra_context_t context = iter->second;
        sgx_status_t sgxretval = SGX_SUCCESS;
        sgx_status_t sgxret = ecall_enclave_ra_close(global_eid, &sgxretval, &context);
        if (sgxretval != SGX_SUCCESS)
        {
          LOG(ERROR) << "enclave ra close failed";
        }
        if (sgxret != SGX_SUCCESS) 
        {
          LOG(ERROR) << "enclave ra close failed";
        }
        g_client_context_map.erase(iter);
      }
    }

    static bool process_client_msg2(const char *request_message, RaResponse *response)
    {
      bool ret = true;
      ra_message *request_msg = (ra_message *)request_message;
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      sgx_ra_msg2_t *msg2 = (sgx_ra_msg2_t *)request_msg->body;
      // generate msg3
      int busy_retry_time = 2;  // retry time
      sgx_status_t sgxret = SGX_SUCCESS; //  sgx ret status
      sgx_ra_context_t context = INT_MAX; // ra context
      uint32_t msg3_size = 0;
      sgx_ra_msg3_t *p_msg3 = NULL; // for sgx ra msg3

      uint32_t rsp_size = 0;
      ra_message *response_msg = NULL;
      // find the context for the client
      auto iter = g_client_context_map.find(uuid_s);
      if (iter == g_client_context_map.end())
      {
        LOG(ERROR) << "can't find client's uuid, unknown client";
        ret = false;
        goto cleanup;
      }
      context = iter->second;
      do
      {
        sgxret = sgx_ra_proc_msg2_ex(&g_selected_key_id, context, global_eid, sgx_ra_proc_msg2_trusted, sgx_ra_get_msg3_trusted, msg2, request_msg->length, &p_msg3, &msg3_size);
      } while (sgxret == SGX_ERROR_BUSY && busy_retry_time--);
      if (!p_msg3)
      {
        LOG(ERROR) << "call sgx_ra_proc_msg2_ex failed ";
        ret = false;
        goto cleanup;
      }
      if (SGX_SUCCESS != sgxret)
      {
        LOG(ERROR) << "call sgx_ra_proc_msg2_ex failed";
        LOG(ERROR) << get_error_message((sgx_status_t)sgxret);
        ret = false;
        goto cleanup;
      }
      else
      {
        LOG(INFO) << "call sgx_ra_proc_msg2_ex success";
      }

      // send msg 3 to client
      rsp_size = sizeof(_ra_message_t) + msg3_size;
      response_msg = (ra_message *)malloc(rsp_size);
      response_msg->type = SERVER_MSG3;
      response_msg->length = msg3_size;
      memcpy(response_msg->uuid, uuid_s.c_str(), UUID_LENGTH);
      memcpy(response_msg->body, (const uint8_t *)p_msg3, msg3_size);

      response->set_message((const char *)response_msg, rsp_size);

    cleanup:
      if (!ret) {
        sendErrorMessageToClient(response);
        removeRaContext(uuid_s);
      }
      if (p_msg3) free(p_msg3);
      if (response_msg) free(response_msg);
      return ret;
    }

    static bool process_client_msg0(const char *request_message, RaResponse *response)
    {
      bool ret = true;
      ra_message *request_msg = (ra_message *)request_message;
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      sgx_status_t sgxret = SGX_SUCCESS;
      sgx_ra_context_t context = INT_MAX;
      auto iter = g_client_context_map.find(uuid_s);

      uint32_t msg1_size = 0;
      ra_message *response_msg = NULL;

      sgx_ra_msg1_t msg1;
      int busy_retry_time = 3;
      if (iter == g_client_context_map.end())
      {
        LOG(ERROR) << "can't find client's uuid, unknown client";
        ret = false;
        goto cleanup;
      }

      // msg0 is the support key id list
      sgxret = sgx_select_att_key_id(request_msg->body, request_msg->length, &g_selected_key_id);
      if (sgxret != SGX_SUCCESS)
      {
        ret = false;
        goto cleanup;
      }
      // generate MSG1
      context = iter->second;

      msg1_size = sizeof(sgx_ra_msg1_t);
      response_msg = (ra_message *)malloc(sizeof(_ra_message_t) + msg1_size);
      // ecall to get msg1 inside enclave
      do
      {
        sgx_ra_get_msg1_ex(&g_selected_key_id, context, global_eid, sgx_ra_get_ga, &msg1);
      } while (SGX_ERROR_BUSY == sgxret && busy_retry_time--);
      if (sgxret != SGX_SUCCESS)
      {
        LOG(ERROR) << "sgx ra get msg1 failed!";
        ret = false;
        goto cleanup;
      }
      response_msg->type = SERVER_MSG1;
      response_msg->length = msg1_size;
      memcpy(response_msg->uuid, uuid_s.c_str(), UUID_LENGTH);
      memcpy(response_msg->body, (const uint8_t *)&msg1, msg1_size);

      // set response
      response->set_message((const char *)response_msg, sizeof(_ra_message_t) + msg1_size);
      cleanup:
      if (!ret) {
        sendErrorMessageToClient(response);
        removeRaContext(uuid_s);
      }
      if (response_msg) free(response_msg);
      return ret;
    }

    static bool sendErrorMessageToClient(RaResponse *response)
    {
      ra_message *response_msg = (ra_message *)malloc(sizeof(_ra_message_t));
      memset(response_msg, 0, sizeof(_ra_message_t));
      response_msg->type = INTERNAL_ERROR;
      if (response_msg)
        free(response_msg);
      response->set_message((const char *)response_msg, sizeof(_ra_message_t));
      return true;
    }

    static bool process_client_close(const char *request_message, RaResponse *response)
    {
      ra_message *request_msg = (ra_message *)request_message;
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      bool ret = true;
      sgx_ra_context_t context = INT_MAX;
      sgx_status_t sgxretval = SGX_SUCCESS;
      sgx_status_t sgxret = SGX_SUCCESS;
      ra_message *response_msg = NULL;
      // find the ra context
      auto iter = g_client_context_map.find(uuid_s);
      if (iter == g_client_context_map.end())
      {
        LOG(ERROR) << "can't find client's uuid, unknown client";
        ret = false;
        goto cleanup;
      }
      removeRaContext(uuid_s);

      // the uint8_t represents the success or failed
      response_msg = (ra_message *)malloc(sizeof(ra_message) + sizeof(uint8_t));
      response_msg->type = SERVER_CLOSE_RESPONSE;
      LOG(INFO) << "client remote attestation context closed";
    cleanup:
      if (!ret)
        sendErrorMessageToClient(response);
      if (response_msg)
        free(response_msg);
      return ret;
    }
  };

} // namespace ra