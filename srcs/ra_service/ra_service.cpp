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
    }
    virtual ~RaServiceImpl() {}

    virtual void Challenge(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done)
    {
      brpc::ClosureGuard done_guard(done);
      brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
      // process challenge msg
      ra_message *request_msg = (ra_message *)request->message().c_str();

      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      bool ret = true;
      sgx_ra_context_t context = INT_MAX;
      boost::uuids::random_generator gen;
      boost::uuids::uuid u = gen();

      uint32_t challenge_rsp_length = 0;
      ra_challenge_response *challenge_rsp = NULL;
      ra_message *response_msg = NULL;

      sgx_status_t sgxretval = SGX_SUCCESS;
      sgx_status_t sgxret = SGX_SUCCESS;

      sgx_ec256_public_t *client_pub = (sgx_ec256_public_t *)request_msg->body;
      if (request_msg->type != CLIENT_CHALLENGE)
      {
        LOG(ERROR) << "request msg type is not client challenge";
        ret = false;
        goto cleanup;
      }
      // init ra context
      sgxret = ecall_enclave_ra_init(global_eid, &sgxretval, false, &context, client_pub);
      if (sgxretval != SGX_SUCCESS)
      {
        LOG(ERROR) << "enclave ra init failed";
        ret = false;
        goto cleanup;
      }
      // generate random uuid for the client
      uuid_s = boost::uuids::to_string(u);
      // save the context for later use
      {
        std::unique_lock<std::mutex> lk(g_context_map_mutex);
        g_client_context_map.insert(std::make_pair(uuid_s, context));
      }
      sgxret = sgx_get_extended_epid_group_id(&g_extended_epid_group_id);
      if (sgxret != SGX_SUCCESS)
      {
        LOG(ERROR) << "get extened epid failed" << get_error_message(sgxret);
        ret = false;
        goto cleanup;
      }
      // generate a response
      challenge_rsp_length = sizeof(_ra_challenge_response_t);
      challenge_rsp = (ra_challenge_response *)malloc(challenge_rsp_length);
      challenge_rsp->extended_group_id = g_extended_epid_group_id;
      response_msg = (ra_message *)malloc(sizeof(_ra_message_t) + challenge_rsp_length);
      response_msg->type = SERVER_CHALLENGE_RESPONSE;
      memcpy(response_msg->uuid, uuid_s.c_str(), UUID_LENGTH);
      response_msg->length = challenge_rsp_length;
      memcpy(response_msg->body, (uint8_t *)challenge_rsp, challenge_rsp_length);
      //set rpc response
      response->set_message((const char *)response_msg, sizeof(_ra_message_t) + challenge_rsp_length);
    cleanup:
      if (!ret)
      {
        sendErrorMessageToClient(response);
        // if the ra context exists erase the context from the map
        removeRaContext(uuid_s);
      }
      if (challenge_rsp)
        free(challenge_rsp);
      if (response_msg)
        free(response_msg);
    }

    virtual void Msg0Process(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done)
    {
      brpc::ClosureGuard done_guard(done);
      brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
      //process msg 0
      bool ret = true;
      ra_message *request_msg = (ra_message *)request->message().c_str();
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      sgx_status_t sgxret = SGX_SUCCESS;
      sgx_ra_context_t context = INT_MAX;
      auto iter = g_client_context_map.find(uuid_s);
      uint32_t msg1_size = 0;
      ra_message *response_msg = NULL;
      sgx_ra_msg1_t msg1;
      int busy_retry_time = 3;
      if (request_msg->type != CLIENT_MSG0)
      {
        LOG(ERROR) << "request msg type is not client challenge";
        ret = false;
        goto cleanup;
      }
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
      memset(&msg1, 0, msg1_size);
      do
      {
        sgx_ra_get_msg1(context, global_eid, sgx_ra_get_ga, &msg1);
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
      response->set_message((const char *)response_msg, sizeof(_ra_message_t) + msg1_size);
    cleanup:
      if (!ret)
      {
        sendErrorMessageToClient(response);
        removeRaContext(uuid_s);
      }
      if (response_msg)
        free(response_msg);
    }

    virtual void Msg2Process(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done)
    {
      brpc::ClosureGuard done_guard(done);
      brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
      //process msg 0
      bool ret = true;
      ra_message *request_msg = (ra_message *)request->message().c_str();
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      sgx_ra_msg2_t *msg2 = (sgx_ra_msg2_t *)request_msg->body;
      // generate msg3
      int busy_retry_time = 2;            // retry time
      sgx_status_t sgxret = SGX_SUCCESS;  //  sgx ret status
      sgx_ra_context_t context = INT_MAX; // ra context
      uint32_t msg3_size = 0;
      sgx_ra_msg3_t *p_msg3 = NULL; // for sgx ra msg3
      uint32_t rsp_size = 0;
      ra_message *response_msg = NULL;
      auto iter = g_client_context_map.find(uuid_s);
      if (request_msg->type != CLIENT_MSG2)
      {
        LOG(ERROR) << "request msg type is not client msg2";
        ret = false;
        goto cleanup;
      }

      if (iter == g_client_context_map.end())
      {
        LOG(WARNING) << "can't find client's uuid, unknown client";
        ret = false;
        goto cleanup;
      }
      context = iter->second;
      do
      {
        sgxret = sgx_ra_proc_msg2(context, global_eid, sgx_ra_proc_msg2_trusted, sgx_ra_get_msg3_trusted, msg2, request_msg->length, &p_msg3, &msg3_size);
      } while (sgxret == SGX_ERROR_BUSY && busy_retry_time--);
      if (!p_msg3)
      {
        LOG(ERROR) << "call sgx_ra_proc_msg2 failed ";
        LOG(ERROR) << get_error_message(sgxret);
        ret = false;
        goto cleanup;
      }
      if (SGX_SUCCESS != sgxret)
      {
        LOG(ERROR) << "call sgx_ra_proc_msg2 failed";
        LOG(ERROR) << get_error_message((sgx_status_t)sgxret);
        ret = false;
        goto cleanup;
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
      if (!ret)
      {
        sendErrorMessageToClient(response);
        removeRaContext(uuid_s);
      }
      if (p_msg3)
        free(p_msg3);
      if (response_msg)
        free(response_msg);
    }

    virtual void Msg4Process(google::protobuf::RpcController *cntl_base, const RaRequest *request, RaResponse *response, google::protobuf::Closure *done)
    {
      bool ret = true;
      brpc::ClosureGuard done_guard(done);
      brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
      ra_message *request_msg = (ra_message *)request->message().c_str();
      std::string uuid_s((const char *)request_msg->uuid, UUID_LENGTH);
      auto iter = g_client_context_map.find(uuid_s);
      ra_message *response_msg = NULL;
      sgx_ra_context_t context = INT_MAX;
      sgx_status_t sgxret = SGX_SUCCESS;

      attestation_status_t enclave_trusted = Trusted;
      sgx_update_info_bit_t update_info;
      sgx_platform_info_t emptyPIB;

      memset(&emptyPIB, 0, sizeof(sgx_platform_info_t));
      ra_msg4_t *msg4 = (ra_msg4_t *)(request_msg->body);
      if (request_msg->type != CLIENT_MSG4)
      {
        LOG(ERROR) << "message type doesn't match";
        ret = false;
        goto cleanup;
      }
      // find the ra context
      if (iter == g_client_context_map.end())
      {
        LOG(ERROR) << "can't find client's uuid, unknown client";
        ret = false;
        goto cleanup;
      }

      //process msg4
      enclave_trusted = msg4->status;
      if (enclave_trusted == Trusted || enclave_trusted == Trusted_ItsComplicated)
      {
        sgxret = sgx_report_attestation_status(&msg4->platformInfoBlob,
                                            enclave_trusted, &update_info);

        /* Check to see if there is an update needed */
        if (sgxret == SGX_SUCCESS)
        {
          LOG(INFO) << "enclave trusted";
        }
        else if (sgxret == SGX_ERROR_UPDATE_NEEDED)
        {
          LOG(INFO) << "The following Platform Update(s) are required to bring this platform's Trusted Computing Base (TCB) back into compliance:";
          if (update_info.pswUpdate)
          {
            LOG(WARNING) << "  * Intel SGX Platform Software needs to be updated to the latest version.";
          }
          if (update_info.csmeFwUpdate)
          {
            LOG(WARNING) << "  * The Intel Management Engine Firmware Needs to be Updated.  Contact your OEM for a BIOS Update.";
          }
          if (update_info.ucodeUpdate)
          {
            LOG(WARNING) << "  * The CPU Microcode needs to be updated.  Contact your OEM for a platform BIOS Update.";
          }
        }
      }
      else
      {
        LOG(WARNING) << "enclave not trusted";
      }
      LOG(INFO) << "remote attestation finished";
      response_msg = (ra_message *)malloc(sizeof(ra_message));
      memset(response_msg, 0, sizeof(ra_message));
      response_msg->type = SERVER_MSG5;
      response->set_message((const char *)response_msg, sizeof(ra_message));

    cleanup:
      if (!ret)
      {
        sendErrorMessageToClient(response);
        removeRaContext(uuid_s);
      }
      if (response)
        free(response_msg);
    }

  private:
    //map message type to process function
    std::map<message_type, process_message_func> func_table_;

    static void removeRaContext(std::string uuid_s)
    {
      std::unique_lock<std::mutex> lk(g_context_map_mutex);
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
      else
      {
        LOG(WARNING) << "can't find the uuids when remove the sgxcontext";
      }
    }

    bool sendErrorMessageToClient(RaResponse *response)
    {
      ra_message *response_msg = (ra_message *)malloc(sizeof(_ra_message_t));
      memset(response_msg, 0, sizeof(_ra_message_t));
      response_msg->type = INTERNAL_ERROR;
      response->set_message((const char *)response_msg, sizeof(_ra_message_t));
      if (response_msg)
        free(response_msg);
      return true;
    }
  };

} // namespace ra