/* MIT License 

* Copyright (c) 2020 jianlinjiang

*/

#include <butil/logging.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
// must use extern "C" 
// otherwise undefine reference ld error happens
extern "C"
{
  void ocall_log(int severity, const char *file, int line, const char *str)
  {
    ::logging::LogMessage(file, line, ::logging::LogSeverity(severity)).stream() << str;
  }

  int prev_layer = -1;
  int fd = -1;
  void ocall_save_aggregation_result(int layer_index, uint8_t *encrypted_data, uint32_t length, uint8_t *tag) {
    std::string filename = "result/" + std::to_string(layer_index);
    if (layer_index != prev_layer) {
      // new layer
      if (fd != -1) close(fd);
      fd = open(filename.c_str(), O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      if (fd == -1) {
        LOG(ERROR) << "open result file failed, filename "<<filename;
        return ;
      }
      prev_layer = layer_index;
    }
    // else old layer, write to the file
    size_t write_bytes = write(fd, &length, 4);
    if (write_bytes != 4) {
      LOG(ERROR) << "write length to file failed";
      return ;
    }
    write_bytes = write(fd, tag, 16);
    if (write_bytes != 16) {
      LOG(ERROR) << "write tag to file failed";
      return ;
    }
    write_bytes = write(fd, encrypted_data, length);
    if (write_bytes != length) {
      LOG(ERROR) << "write encrypted data to file failde";
      return ;
    }
  }
}
