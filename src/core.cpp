#include "core.hpp"

#include "tcp_server.hpp"
#include "empty_ptree.hpp"
#include "log.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/settings.hpp>
#include <boost/log/utility/setup/from_settings.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/utility/setup/filter_parser.hpp>
#include <boost/make_shared.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range/adaptor/map.hpp>

namespace eiptnd {

namespace app = boost::application;

void
init_logging()
{
  namespace attrs = boost::log::attributes;

  BOOST_AUTO(log_core, boost::log::core::get());
  log_core->add_global_attribute("TimeStamp", attrs::local_clock());
  log_core->add_global_attribute("RecordID", attrs::counter<unsigned long>());

  boost::log::register_simple_formatter_factory<logging::severity_level, char>("Severity");
  boost::log::register_simple_filter_factory<logging::severity_level, char>("Severity");

  boost::log::settings log_settings;
  //log_settings["Core.Filter"] = "%Severity% >= normal";
  log_settings["Sinks.Console.Destination"] = "Console";
  log_settings["Sinks.Console.Format"] = "[%TimeStamp%] <%Severity%>\t[%Channel%] - %Message%";
  log_settings["Sinks.Console.AutoFlush"] = true;
  log_settings["Sinks.Console.Asynchronous"] = false;
  boost::log::init_from_settings(log_settings);
}

core::core(app::context& context)
  : log_(boost::log::keywords::channel = "core")
  , context_(context)
  , vm_(*context.find<boost::program_options::variables_map>())
  , webroot_(vm_["dir"].as<std::string>())
  , is_shutdowning_(false)
{
}

core::~core()
{
  BOOST_AUTO(logging_core, boost::log::core::get());
  logging_core->flush();
  logging_core->remove_all_sinks();
}

int
core::operator()()
{
  int ret = EXIT_SUCCESS;
  bool is_catch = false;
  try {
    init_logging();
    run();
  }
  catch (...) {
    is_catch = true;
    BOOST_LOG_SEV(log_, logging::critical)
      << boost::current_exception_diagnostic_information();
  }

  boost::log::core::get()->flush();

  if (is_catch) {
    BOOST_LOG_SEV(log_, logging::global)
      << "Shutting down after an unrecoverable error";

    stop();
    ret = EXIT_FAILURE;
  }

  return ret;
}

bool
core::stop()
{
  BOOST_LOG_SEV(log_, logging::global) << "Caught signal to stop";

  boost::log::core::get()->flush();

  if (is_shutdowning_) {
    BOOST_LOG_SEV(log_, logging::global) << "Forced to shutdown";

    if (io_service_) {
      io_service_->stop();
    }

    boost::log::core::get()->flush();

    return true;
  }

  is_shutdowning_ = true;

  BOOST_LOG_SEV(log_, logging::normal) << "Freeing listeners";
  BOOST_FOREACH(const boost::weak_ptr<tcp_server>& listener, listeners_) {
    BOOST_AUTO(p, listener.lock());
    if (p) {
      p->cancel();
    }
  }

  BOOST_LOG_SEV(log_, logging::notify)
    << "Cleanup is done. Waiting for io_service release...";

  BOOST_LOG_SEV(log_, logging::normal)
    << "The io_service is still in use " << io_service_.use_count() << " objects";

  boost::log::core::get()->flush();

  return true; // return true to stop, false to ignore
}

void
core::run()
{
  std::size_t thread_pool_size = vm_["num-threads"].as<std::size_t>();
  BOOST_LOG_SEV(log_, logging::info)
      << "Asio thread pool size: " << thread_pool_size;

  io_service_ = boost::make_shared<boost::asio::io_service>(thread_pool_size);

  string_vector bind_list = vm_["host"].as<string_vector>();
  unsigned short port_num = vm_["port"].as<unsigned short>();

  try {
    BOOST_FOREACH(const std::string& address, bind_list) {
      BOOST_LOG_SEV(log_, logging::normal)
        << "TCP listener at " << address << ":" << port_num << " was created";

      boost::shared_ptr<tcp_server> listener =
          boost::make_shared<tcp_server>(boost::ref(*this), address, port_num);
      listeners_.push_back(listener);
      listener->start_accept();
    }
  }
  catch (const boost::system::system_error& e) {
    BOOST_LOG_SEV(log_, logging::critical)
      << e.what() << " (" << e.code().value() << ")";

    throw;
  }

  if (thread_pool_size > 1) {
    boost::thread_group threads;
    for (std::size_t i = 0; i < thread_pool_size; ++i) {
      threads.create_thread(
          boost::bind(&boost::asio::io_service::run, io_service_));
    }

    threads.join_all();
  }
  else {
    io_service_->run();
  }


  BOOST_LOG_SEV(log_, logging::notify) << "All threads are done";
}

} // namespace eiptnd