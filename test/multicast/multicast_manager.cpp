#include "multicast_manager.h"
#include <iostream>
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

constexpr int max_message_count = 100;
const char* node_data_path = "/home/dbc/dbc_mining_node/dat/node.dat";

inline std::string random_string(size_t length) {
    auto randchar = []() -> char {
        const char charset[] = "0123456789"
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[static_cast<size_t>(rand()) % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

inline std::string get_node_id()
{
    std::string node_id;
    if (boost::filesystem::exists(node_data_path)) {
        boost::program_options::variables_map node_dat_args;
        try
        {
            boost::program_options::options_description node_dat_opts("node.dat");
            node_dat_opts.add_options()
                ("node_id", boost::program_options::value<std::string>(), "")
                ("node_private_key", boost::program_options::value<std::string>(), "")
                ("pub_key", boost::program_options::value<std::string>(), "")
                ("priv_key", boost::program_options::value<std::string>(), "");
            std::ifstream node_dat_ifs(node_data_path);
            boost::program_options::store(
                boost::program_options::parse_config_file(node_dat_ifs, node_dat_opts),
                node_dat_args);
            boost::program_options::notify(node_dat_args);
            if (node_dat_args.count("node_id")) {
                node_id = node_dat_args["node_id"].as<std::string>();
            }
        }
        catch (const boost::exception & e)
        {
            std::cout << "parse node.dat error: " << diagnostic_information(e);
        }
    }
    if (node_id.empty()) return random_string(10);
    return node_id;
}

multicast_manager::multicast_manager(boost::asio::io_context& io_context,
        const boost::asio::ip::address& listen_address,
        const boost::asio::ip::address& multicast_address,
        short multicast_port)
    : receiver_socket_(io_context)
    , endpoint_(multicast_address, multicast_port)
    , sender_socket_(io_context, endpoint_.protocol())
    , timer_(io_context)
    , message_count_(0)
{
    // Create the socket so that multiple may be bound to the same address.
    boost::asio::ip::udp::endpoint listen_endpoint(
        listen_address, multicast_port);
    receiver_socket_.open(listen_endpoint.protocol());
    receiver_socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    receiver_socket_.bind(listen_endpoint);

    // Join the multicast group.
    receiver_socket_.set_option(
        boost::asio::ip::multicast::join_group(multicast_address));

    do_receive();

    node_id_ = get_node_id();
    do_send();
}

void multicast_manager::do_receive()
{
    receiver_socket_.async_receive_from(
        boost::asio::buffer(data_), sender_endpoint_,
        [this](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::cout.write(data_.data(), length);
            std::cout << " from " << sender_endpoint_.address().to_string()
                << ":" << sender_endpoint_.port();
            std::cout << std::endl;

            do_receive();
          }
        });
}

void multicast_manager::do_send()
{
    std::ostringstream os;
    // os << "Message " << message_count_++;
    os << "node: " << node_id_ << " ,times: " << message_count_++;
    message_ = os.str();

    sender_socket_.async_send_to(
        boost::asio::buffer(message_), endpoint_,
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && message_count_ < max_message_count)
            do_timeout();
        });
}

void multicast_manager::do_timeout()
{
    timer_.expires_after(std::chrono::seconds(1));
    timer_.async_wait(
        [this](boost::system::error_code ec)
        {
          if (!ec)
            do_send();
        });
}
