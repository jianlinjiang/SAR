// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef RA_MESSAGE_H
#define RA_MESSAGE_H
#include <stdint.h>
#include "sgx_key_exchange.h"
enum message_type
{
  CLIENT_CHALLENGE = 0,
  SERVER_CHALLENGE_RESPONSE = 1,
  CLIENT_MSG0 = 2,
  SERVER_MSG1 = 3,
  CLIENT_MSG2 = 4,
  SERVER_MSG3 = 5,
  CLIENT_MSG4,
  SERVER_MSG5,
  CLIENT_TRANSMIT_WEIGHT,
  SERVER_TRANSMIT_RESPONSE,
  TEST = 100,
  INTERNAL_ERROR = 101,
};
const static int UUID_LENGTH = 36;
namespace ra
{

  typedef enum
  {
    NotTrusted = 0,
    NotTrusted_ItsComplicated,
    Trusted_ItsComplicated,
    Trusted
  } attestation_status_t;

  typedef struct _ra_msg4_struct
  {
    attestation_status_t status;
    sgx_platform_info_t platformInfoBlob;
  } ra_msg4_t;

  
  typedef struct _ra_message_t
  {
    message_type type;
    uint8_t uuid[UUID_LENGTH];
    uint32_t length;
    uint8_t body[];
  } ra_message;
  typedef struct _ra_challenge_response_t
  {
    uint32_t extended_group_id;
  } ra_challenge_response;

} // namespace ra

#endif
