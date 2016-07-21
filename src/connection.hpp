#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include "log.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>


namespace eiptnd {

class core;
class http_connection;

/// Represents a single connection from a client.
class connection
  : public boost::enable_shared_from_this<connection>
  , private boost::noncopyable
{
  typedef http_connection process_handler_t;
public:
//private:
  explicit connection(core const& core);

  //friend boost::shared_ptr<connection> boost::make_shared(core&);
  //friend boost::detail::sp_if_not_array<connection>::type /*::boost::*/make_shared(core&);

public:
  ~connection();

  static boost::shared_ptr<connection> create(core const& core)
  {
    boost::shared_ptr<connection> p = boost::make_shared<connection>(core);
    p->weak_this_ = p;
    return p;
  }

  std::string const& get_webroot() const;

  /// Get the socket associated with the connection.
private: boost::asio::ip::tcp::socket& socket() { return socket_; }
public:  boost::asio::ip::tcp::socket const& socket() const { return socket_; }

  /// Get the translator associated with the connection.
private: boost::weak_ptr<process_handler_t>& process_handler() { return process_handler_; }
public:  boost::weak_ptr<process_handler_t> const& process_handler() const { return process_handler_; }

  /// Start the first asynchronous operation for the connection.
  void on_connection();

  /// Initiate graceful connection closure.
  void close();

  /// Reading API.
  void do_read_some(const boost::asio::mutable_buffer& buffer);
  void do_read_until(boost::asio::streambuf& sbuf, const std::string& delim);
  void do_read_at_least(boost::asio::streambuf& sbuf, std::size_t minimum);

  /// Writing API.
  void do_write(const boost::asio::const_buffer& buffer);
  void do_write_cb(const boost::asio::const_buffer& buffer, boost::function<void()> f);

  /// Getters for statistics data
  boost::uint64_t bytes_sent() const          { return sent_bytes_;     }
  boost::uint64_t bytes_recieved() const      { return recieved_bytes_; }
  boost::uint64_t count_reads() const         { return reads_count_;    }
  boost::uint64_t count_writes() const        { return writes_count_;   }
  boost::asio::ip::tcp::endpoint remote_endpoint() const
  { return remote_endpoint_; }

private:
  /// Handle completion of a read operation.
  void handle_read(
      boost::shared_ptr<process_handler_t> process_handler,
      const boost::system::error_code& ec,
      std::size_t bytes_transferred);

  /// Handle completion of a write operation.
  void handle_write(
      boost::shared_ptr<process_handler_t> process_handler,
      const boost::system::error_code& ec,
      std::size_t bytes_transferred);
  void handle_write_cb(
      boost::shared_ptr<process_handler_t> process_handler,
      boost::function<void()> f, const boost::system::error_code& ec,
      std::size_t bytes_transferred);

  /// Post passed function, wrapped in connection's strand, to io_service
  void post_in_strand(boost::function<void()> f);

  /// Dispatch passed function, wrapped in connection's strand, with io_service
  void dispatch_in_strand(boost::function<void()> f);

  /// Logger instance and attributes.
  logging::logger log_;
  boost::log::attribute_set::iterator net_raddr_;

  ///
  core const& core_;

  boost::shared_ptr<boost::asio::io_service> io_service_;

  /// Strand to ensure the connection's handlers are not called concurrently.
  boost::asio::io_service::strand strand_;

  /// Socket for the connection.
  boost::asio::ip::tcp::socket socket_;

  /// The handler used to process the data.
  boost::weak_ptr<process_handler_t> process_handler_;

  /// Statistics data counters
  /// (It's safe to declare non atomic because they are changed in strands)
  boost::uint64_t sent_bytes_, recieved_bytes_;
  boost::uint64_t reads_count_, writes_count_;

  /// TODO: replace by weak_from_this() at migrating to >= boost 1.58
  boost::weak_ptr<connection> weak_this_;

  /// Cache the remote endpoint value as socket.remote_endpoint()
  /// can fail in some situations (is not only when socket is closed).
  /// This should be ok because session is binded to single socket.
  boost::asio::ip::tcp::endpoint remote_endpoint_;

  /// Server acceptor needs the access to nonconst socket.
  friend class tcp_server;
};

typedef boost::shared_ptr<connection> connection_ptr;

} // namespace eiptnd

#endif // CONNECTION_HPP
