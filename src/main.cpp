#include "core.hpp"

#include <iostream>
#include <string>
#include <boost/application.hpp>
#include <boost/program_options.hpp>

int main(int argc, char* argv[])
{
  //setup_crash_handler();

  namespace app = boost::application;
  namespace po = boost::program_options;

  boost::shared_ptr<po::variables_map> vm = boost::make_shared<po::variables_map>();

  po::options_description general("General options");
  general.add_options()
    ("help", "show this help message")
    ("foreground,F", "run in foreground mode")
  ;

  std::size_t num_threads = std::max(1u, boost::thread::hardware_concurrency());
  po::options_description network("Network Options");
  network.add_options()
    ("host,h", po::value<string_vector>()
                 ->default_value(string_vector(1, "0.0.0.0"), "0.0.0.0")
                 ->multitoken()->value_name("ip"), "bind address")
    ("port,p", po::value<unsigned short>()->default_value(80)
                 ->value_name("port"), "bind port")
    ("dir,d", po::value<std::string>()
                ->default_value("./www")
                ->value_name("directory"), "web root directory")
    ("num-threads", po::value<std::size_t>()->default_value(num_threads)
       ->value_name("N"), "number of connection handler threads count")
  ;

  po::options_description desc("Allowed Options");
  desc.add(general).add(network);

#if defined(BOOST_WINDOWS_API)
  po::options_description service("Service Options");

  service.add_options()
    ("install", po::value<std::string>()
                  ->value_name("options"), "install service")
    ("uninstall", "unistall service")
  ;

  desc.add(service);
#endif

  try {
    po::store(po::command_line_parser(argc, argv)
        .options(desc).run(), *vm);
    po::notify(*vm);
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    std::cout << desc << std::endl;
    //std::cerr << boost::diagnostic_information(e) << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "stepic-mp-final" "\n" << std::endl;

  if (vm->count("help")) {
    std::cout << desc << std::endl;
    return EXIT_SUCCESS;
  }

  int result = EXIT_SUCCESS;
  boost::system::error_code ec;

  app::context app_ctx;
  {
    app_ctx.insert(vm);
    app::auto_handler<eiptnd::core> app_core(app_ctx);

    if (vm->count("foreground")) {
      result = app::launch<app::common>(app_core, app_ctx, ec);
    }
    else {
      std::cout << "Deamonizing..." << std::endl;
      result = app::launch<app::server>(app_core, app_ctx, ec);
    }

    if (ec) {
      std::cerr << "[main::Error] " << ec.message()
                << " (" << ec.value() << ")" << std::endl;
      result = EXIT_FAILURE;
    }
  }

  std::cout << "Bye!" << std::endl;

  return result;
}
