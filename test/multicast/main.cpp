#include <iostream>
#include <string>
#include <boost/program_options.hpp>

#include "multicast_manager.h"

static const char* default_listen_address = "0.0.0.0";
static const char* default_multicast_address = "239.255.0.1";
constexpr short default_multicast_port = 30001;

int main(int argc, char* argv[]) {
    std::string multicast_address = default_multicast_address;
    short multicast_port = default_multicast_port;

    boost::program_options::options_description opts("multicast test command options");
    opts.add_options()
        ("address", boost::program_options::value<std::string>(), "multicast address")
        ("port", boost::program_options::value<short>(), "multicast port")
        ("help", "command options help");

    try {
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, opts), vm);
        boost::program_options::notify(vm);

        if (vm.count("address")) {
            multicast_address = vm["address"].as<std::string>();
        }
        if (vm.count("port")) {
            multicast_port = vm["port"].as<short>();
        }
        if (vm.count("help") > 0) {
            std::cout << opts;
            return 0;
        }
    }
    catch (const std::exception &e) {
        std::cout << "invalid command option " << e.what() << std::endl;
        std::cout << opts;
        return 0;
    }

    try {
        srand((int)time(0));
        boost::asio::io_context io_context;
        multicast_manager mm(io_context,
            boost::asio::ip::make_address(default_listen_address),
            boost::asio::ip::make_address(multicast_address),
            multicast_port);
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}