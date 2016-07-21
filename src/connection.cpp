#include "connection.hpp"

#include "core.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/manipulators/dump.hpp>

#include "http/http_connection.hpp"


namespace eiptnd {

std::string const& connection::get_webroot() const
{ return core_.get_webroot(); }


connection::connection(core const& core)
  : log_(boost::log::keywords::channel = "connection")
  , core_(core)
  , io_service_(core_.get_ios())
  , strand_(*io_service_)
  , socket_(*io_service_)
  , sent_bytes_(0)
  , recieved_bytes_(0)
  , reads_count_(0)
  , writes_count_(0)
{
  /// NOTE: There is no real conection here, only waiting for it.
}

connection::~connection()
{
  BOOST_LOG_SEV(log_, logging::info) << "Session is destroyed";
}

void
connection::on_connection()
{
  remote_endpoint_ = socket_.remote_endpoint();

  boost::log::attributes::constant<std::string> addr(
      boost::lexical_cast<std::string>(remote_endpoint_));
  net_raddr_ = log_.add_attribute("RemoteAddress", addr).first;

  BOOST_LOG_CHANNEL_SEV(log_, "connection@" + addr.get(), logging::info)
    << "Connection accepted";

  auto ptr = boost::make_shared<process_handler_t>(shared_from_this());
  process_handler_ = ptr;
  ptr->handle_start();
}

void
connection::post_in_strand(boost::function<void()> f)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "post_in_strand()";

  /// TODO: Maybe add inderection level with exception catching for safety?
  io_service_->post(strand_.wrap(f));
}

void
connection::dispatch_in_strand(boost::function<void()> f)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "dispatch_in_strand()";

  /// TODO: Maybe add inderection level with exception catching for safety?
  io_service_->dispatch(strand_.wrap(f));
}

void
connection::do_read_at_least(boost::asio::streambuf& sbuf, std::size_t minimum)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "do_read_at_least(): " << minimum << " bytes";

  boost::asio::async_read(socket_, sbuf, boost::asio::transfer_at_least(minimum),
      strand_.wrap(
        boost::bind(&connection::handle_read, shared_from_this(), process_handler_.lock(), _1, _2)));
}

void
connection::do_read_until(boost::asio::streambuf& sbuf, const std::string& delim)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "do_read_until(): " << boost::log::dump(delim.data(), delim.size());

  boost::asio::async_read_until(socket_, sbuf, delim,
      strand_.wrap(
        boost::bind(&connection::handle_read, shared_from_this(), process_handler_.lock(), _1, _2)));
}

void
connection::do_read_some(const boost::asio::mutable_buffer& buffers)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "do_read_some(): " << boost::asio::buffer_size(buffers) << " bytes";

  socket_.async_read_some(boost::asio::mutable_buffers_1(buffers),
      strand_.wrap(
        boost::bind(&connection::handle_read, shared_from_this(), process_handler_.lock(), _1, _2)));
}

void
connection::do_write(const boost::asio::const_buffer& buffers)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "do_write(): " << boost::asio::buffer_size(buffers) << " bytes";

  boost::asio::async_write(socket_,
      boost::asio::const_buffers_1(buffers),
      strand_.wrap(
        boost::bind(&connection::handle_write, shared_from_this(), process_handler_.lock(), _1, _2)));
}

void
connection::do_write_cb(const boost::asio::const_buffer& buffers, boost::function<void()> f)
{
  BOOST_LOG_SEV(log_, logging::flood)
    << "do_write_cb(): " << boost::asio::buffer_size(buffers) << " bytes";

  boost::asio::async_write(socket_,
      boost::asio::const_buffers_1(buffers),
      strand_.wrap(
        boost::bind(&connection::handle_write_cb, shared_from_this(), process_handler_.lock(), f, _1, _2)));
}

void
connection::handle_read(
    boost::shared_ptr<process_handler_t> process_handler,
    const boost::system::error_code& ec,
    std::size_t bytes_transferred)
{
  if (!ec /*|| bytes_transferred > 0*/) {
    BOOST_LOG_SEV(log_, logging::flood)
      << "handle_read(): " << bytes_transferred << " bytes";

    ++reads_count_;
    recieved_bytes_ += bytes_transferred;
    try {
      process_handler->handle_read(bytes_transferred);
    }
    catch (...) {
      BOOST_LOG_SEV(log_, logging::critical)
        << "Exception in translator handle_read(): "
        << boost::current_exception_diagnostic_information();
      close();
    }
  }
  else if (ec == boost::asio::error::eof) {
    BOOST_LOG_SEV(log_, logging::debug)
      << "Connection has been closed by the remote endpoint";
  }
  else if (ec == boost::asio::error::operation_aborted) {
    BOOST_LOG_SEV(log_, logging::debug)
      << "Connection unexpectedly closed";
  }
  else {
    BOOST_LOG_SEV(log_, logging::error)
      << "Reading failed: " << ec.message() << " (" << ec.value() << ")";
  }
}

void
connection::handle_write(
    boost::shared_ptr<process_handler_t> process_handler,
    const boost::system::error_code& ec,
    std::size_t bytes_transferred)
{
  if (!ec) {
    BOOST_LOG_SEV(log_, logging::flood)
      << "handle_write(): " << bytes_transferred << " bytes";

    ++writes_count_;
    sent_bytes_ += bytes_transferred;
    try {
      process_handler->handle_write();
    }
    catch (...) {
      BOOST_LOG_SEV(log_, logging::critical)
        << "Exception in translator handle_write(): "
        << boost::current_exception_diagnostic_information();
      close();
    }
  }
  else {
    BOOST_LOG_SEV(log_, logging::error)
      << "Writing failed: " << ec.message() << " (" << ec.value() << ")";
  }
}

void
connection::handle_write_cb(
    boost::shared_ptr<process_handler_t> /*process_handler*/,
    boost::function<void()> f, const boost::system::error_code& ec,
    std::size_t bytes_transferred)
{
  if (!ec) {
    BOOST_LOG_SEV(log_, logging::flood)
      << "handle_write_cb(): " << bytes_transferred << " bytes";

    ++writes_count_;
    sent_bytes_ += bytes_transferred;
    try {
      f();
    }
    catch (...) {
      BOOST_LOG_SEV(log_, logging::critical)
        << "Exception in translator write_callback(): "
        << boost::current_exception_diagnostic_information();
      close();
    }
  }
  else {
    BOOST_LOG_SEV(log_, logging::error)
      << "Writing failed: " << ec.message() << " (" << ec.value() << ")";
  }
}

void
connection::close()
{
  BOOST_LOG_SEV(log_, logging::trace) << "Closing connection";

  boost::system::error_code ignored_ec;
  socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
  socket_.close();
}

} // namespace eiptnd
