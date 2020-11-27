// MIT License

// Copyright (c) 2020 jianlinjiang

#ifndef ENCLAVE_UTIL_H
#define ENCLAVE_UTIL_H

#include "enclave_t.h"
typedef int LogServity;
const LogServity INFO = 0;
const LogServity NOTICE= 1;
const LogServity WARRNING = 2;
const LogServity ERROR = 3;
const LogServity FATAL = 4;

#define LOG(severity, file, line, str) \
  ocall_log(severity, file, line, str);


#endif