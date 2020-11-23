// MIT License

// Copyright (c) 2020 jianlinjiang

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef NETWORK_REQUEST_PARSER_HPP
#define NETWORK_REQUEST_PARSER_HPP

#include <stdint.h>

namespace network {
  namespace server {
    struct request;
    
    class request_parser {
      public:
        request_parser();

        void reset();

        // whole: request package
        // begin: the begin package
        // middle: the middle package
        // end: the end package
        enum result_type {whole, begin, middle, end, error};

        // pre_process to check the buffer is completed.
        result_type pre_process(const uint8_t* buffer, const uint32_t length);

        // parse the buffer to a request
        void parse(request* req, uint8_t* buffer, uint32_t length);

      private:
        result_type consume(request& req, uint32_t* buffer, uint32_t length);

        enum state {
          package_begin, 
          package_middle,
          package_end
        } state_;
    };
  }
}

#endif