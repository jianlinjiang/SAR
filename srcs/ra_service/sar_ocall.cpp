/* MIT License 

* Copyright (c) 2020 jianlinjiang

*/

#include <butil/logging.h>
// must use extern "C" 
// otherwise undefine reference ld error happens
extern "C"
{
  void ocall_log(int severity, const char *file, int line, const char *str)
  {
    ::logging::LogMessage(file, line, ::logging::LogSeverity(severity)).stream() << str;
  }
}
