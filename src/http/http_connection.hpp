#ifndef HTTP_CONNECTION_HPP
#define HTTP_CONNECTION_HPP

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

#include "../connection.hpp"


namespace eiptnd {

class http_connection
  : public boost::enable_shared_from_this<http_connection>
  , private boost::noncopyable
{
public:
  http_connection(boost::shared_ptr<connection> connection)
    : log_(boost::log::keywords::channel = "connection")
    , conn_(boost::move(connection))
  {
  }

  void handle_start();
  void handle_read(std::size_t bytes_transferred);
  void handle_write();

  template <typename Iterator>
  bool process_request(Iterator & first, Iterator const& last);

  void make_simple_answer(unsigned short code,
                          std::string const& repl,
                          std::string const& body);

  void send_file(std::string const& url);

private:
  /// Logger instance and attributes.
  logging::logger log_;

  boost::shared_ptr<connection> conn_;

  /// Buffer for incoming data.
  boost::asio::streambuf in_buf_;
};

}

#endif // HTTP_CONNECTION_HPP
