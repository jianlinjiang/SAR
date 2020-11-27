// MIT License

// Copyright (c) 2020 jianlinjiang

#include <stdint.h>
enum message_type {
  CLIENT_CHALLENGE = 0,
  SERVER_CHALLENGE_RESPONSE = 1,
  SERVER_MSG0 = 2,
};

const static int UUID_LENGTH = 16;

typedef struct _ra_message_t {
  message_type type;
  uint8_t uuid[UUID_LENGTH];
  uint32_t length;
  uint8_t body[];
} ra_message;