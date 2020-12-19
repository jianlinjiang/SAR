// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef SAR_MESSAGE_H
#define SAR_MESSAGE_H
#include <stdint.h>
#include "ra_message.h"
namespace sar
{
  enum file_flag {
    START = 0,
    MIDDLE = 1,
    END
  };
  typedef struct _transmit_message_t
  {
    message_type type;
    file_flag flag;
    uint8_t uuid[UUID_LENGTH];
    uint32_t layer_num; // layer index for each weight
    uint32_t length;
    uint8_t align[4]; 
    uint8_t body[];
  } transmit_message;
  typedef struct _transmit_response_t {
    message_type type;
  } transmit_response;
} // namespace sar

#endif