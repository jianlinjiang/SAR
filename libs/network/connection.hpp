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

#ifndef NETWORK_CONNECTION_HPP
#define NETWORK_CONNECTION_HPP

#include <array>
#include <memory>
#include <vector>
#include <asio.hpp>
#include "reply.hpp"
#include "request.hpp"
#include "request.hpp"
#include "request_handler.hpp"
#include ""
class connection_manager;
namespace network {
  namespace server {
    class connection : public std::enable_shared_from_this<connection>
    {
      public:
        connection(const connection &) = delete;
        connection& operator=(const connection &) = delete;
        
        explicit connection(asio::ip::tcp::socket socket, connection_manager& manager, request_handler& handler);

        // Start the first asynchronous operation for the connection.
        void start();

        // Stop all asynchronous operations associated with the connection.
        void stop();
      private:
        void do_read();

        void do_write();

        asio::ip::tcp::socket socket_;

        connection_manager& connection_manager_;

        request_handler& request_handler_;

        // buffer for reading or writing one time
        // if the message's length is larger than REQUEST_MAX_LENGTH,
        // read multiple times and copy the content to entire_conten_.
        std::array<uint8_t, REQUEST_MAX_LENGTH> buffer_;

        // hold the entire request content
        std::vector<uint8_t> entire_content_;

        request request_;

        request_

        reply reply_;
    };

    typedef std::shared_ptr<connection> connection_ptr;
  }
}


#endif