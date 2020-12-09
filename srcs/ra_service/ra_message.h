// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef RA_MESSAGE_H
#define RA_MESSAGE_H
#include <stdint.h>
#include "sgx_key_exchange.h"
namespace ra
{
  enum message_type
  {
    CLIENT_CHALLENGE = 0,
    SERVER_CHALLENGE_RESPONSE = 1,
    CLIENT_MSG0 = 2,
    SERVER_MSG1 = 3,
    CLIENT_MSG2 = 4,
    SERVER_MSG3 = 5,
    CLIENT_CLOSE_MSG = 10,
    SERVER_CLOSE_RESPONSE = 11,
    TEST = 100,
    INTERNAL_ERROR = 101,
  };

  const static int UUID_LENGTH = 36;
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
