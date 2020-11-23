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

#include "connection.hpp"
#include <vector>
#include <utility>
#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace network
{
  namespace server
  {
    connection::connection(asio::ip::tcp::socket socket, connection_manager &manager, request_handler &handler)
        : socket_(std::move(socket)),
          connection_manager_(manager),
          request_handler_(handler)
    {
    }

    void connection::start()
    {
      do_read();
    }

    void connection::stop()
    {
      socket_.close();
    }

    void connection::do_read()
    {
      auto self(shared_from_this());
      socket_.async_read_some(asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytes_transferred){
        if (!ec) {

        } else if (ec != asio::error::operation_aborted) {
          
        }
      });
    }
  } // namespace server
} // namespace network
