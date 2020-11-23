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

#ifndef NETWORK_REQUEST_HPP
#define NETWORK_REQUEST_HPP

#include <stdint.h>

namespace network
{
  namespace server
  {
    const static uint32_t REQUEST_TYPE_LENGTH = 32;

    const uint8_t *REQUEST_TYPE_ONE= (uint8_t*)"0000_0000_0000_0000_0000_000000";

    bool request_type_equal(const uint8_t *target, const uint8_t* type, const uint32_t length);
    // max length 8192 
    // type 32 bytes
    // is_start 1 byte, is_end 1 byte, align 2 bytes
    // content_length 4 bytes
    // max length for content is 8192 - 40 = 8152 bytes
    struct request
    {
      uint8_t type[REQUEST_TYPE_LENGTH];   // request type
      uint8_t is_start;   // start flag
      uint8_t is_end;     // end flag
      uint8_t align[2];   // align 
      uint32_t content_length;
      uint8_t content[];  
    };
    
    const static uint32_t REQUEST_MAX_LENGTH = 8192;
    const static uint32_t REQUEST_MAX_CONTENT_LENGTH = REQUEST_MAX_LENGTH - sizeof(request);
    
    
  } // namespace server
} // namespace network

#endif