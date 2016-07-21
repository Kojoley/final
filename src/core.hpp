#ifndef CORE_HPP
#define CORE_HPP

#include "log.hpp"
#include "tcp_server.hpp"

#include <vector>
#include <boost/application/context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/program_options/variables_map.hpp>

typedef std::vector<std::string> string_vector;

namespace eiptnd {

class plugin_factory;
class translator_manager;

class core
{
public:
  core(boost::application::context& context);
  ~core();

  /// Handles run signal.
  int operator()();

  /// Handles stop signal.
  bool stop();

  boost::shared_ptr<boost::asio::io_service> const& get_ios() const
  { return io_service_; }

  std::string const& get_webroot() const
  { return webroot_; }

private:
  /// Daemon runner.
  void run();

  /// Logger instance and attributes.
  logging::logger log_;

  /// Application context.
  boost::application::context& context_;

  /// Variables map.
  boost::program_options::variables_map& vm_;

  /// Boost.Asio Proactor.
  boost::shared_ptr<boost::asio::io_service> io_service_;

  ///
  std::vector<boost::weak_ptr<tcp_server> > listeners_;

  std::string webroot_;

  /// Flags if daemon currently in shutdowning phase.
  bool is_shutdowning_;
};

} // namespace eiptnd

#endif // CORE_HPP
