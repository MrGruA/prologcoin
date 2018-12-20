#pragma once

#ifndef _pow_server_hpp
#define _pow_server_hpp

#include <array>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/socket_acceptor_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>

#include "star.hpp"
#include "vec3.hpp"
#include "fxp.hpp"

namespace prologcoin { namespace pow {

/*

DOUBLE
id=12167299 x=154 y=2135
id=12604631 x=896 y=2103
id=42653494 x=1220 y=1791
id=10929535 x=1768 y=1433
id=26613849 x=1745 y=904
id=13695352 x=2605 y=684
id=14503809 x=2861 y=1248

FXP
id=12167299 x=156 y=2134
id=12604631 x=897 y=2103
id=42653494 x=1222 y=1791
id=10929535 x=1769 y=1433
id=26613849 x=1746 y=904
id=13695352 x=2606 y=684
id=14503809 x=2862 y=1247
FOUND...


DOUBLE

id=12167299 x=38 y=533
id=12604631 x=224 y=525
id=42653494 x=305 y=447
id=10929535 x=442 y=358
id=26613849 x=436 y=226
id=13695352 x=651 y=171
id=14503809 x=715 y=312

FXP

id=12167299 x=38 y=533
id=12604631 x=224 y=525
id=42653494 x=305 y=447
id=10929535 x=442 y=358
id=26613849 x=436 y=226
id=13695352 x=651 y=171
id=14503809 x=715 y=312


*/

// typedef fxp1648 arith_t;
typedef double arith_t;

class connections;
class pow_server;

class connection : public std::enable_shared_from_this<connection> {
public:

    connection(const connection &) = delete;
    void operator =(const connection &) = delete;

    explicit connection(boost::asio::ip::tcp::socket socket,
			connections &conns);
    ~connection();

    void start();
    void stop();

private:
    const std::string & home_dir() const;
    const std::string & read_file(const std::string &file) const;

    void do_read();
    void do_write();
    void parse(char *s, size_t n);
    void parse_http();
    void parse_header();
    void parse_content();
    void parse_consume(size_t n);
    void parse_consume();
    void do_action();
    
    template<typename T> T lexical_cast_no_error(const std::string &str) {
	try {
	    return boost::lexical_cast<T>(str);
	} catch (...) {
	    return T(0);
	}
    }

    void do_reply_ok(const std::string &content);
    void do_error();

    boost::asio::ip::tcp::socket socket_;
    connections &connections_;
    std::array<char, 8192> buffer_;
    std::array<char, 8192> parsed_;
    size_t parsed_index_;
    size_t parsed_new_line_;

    enum state_t {
	PARSE_HTTP,
	PARSE_HEADER,
	PARSE_CONTENT,
	PARSE_DONE,
	PARSE_ERROR
    } state_;

    enum method_t {
	METHOD_UNKNOWN,
	METHOD_GET
    } method_;

    std::string uri_;
    std::string ver_;

    std::vector<std::pair<std::string, std::string> > header_;
    std::unordered_map<std::string, std::string> header_map_;
    size_t content_length_ = 0;
    std::string reply_;
    bool keep_alive_;
};

typedef std::shared_ptr<connection> connection_ptr;

class connections {
public:
    connections(class pow_server &pows);

    void start(connection_ptr c);
    void stop(connection_ptr c);
    void stop_all();

    inline class pow_server & pow_server() {
	return pow_server_;
    }    
    inline const class pow_server & pow_server() const {
	return pow_server_;
    }

private:
    class pow_server &pow_server_;
    std::set<connection_ptr> connections_;
};

class pow_server {
public:
    explicit pow_server(const std::string &address, const std::string &port,
			const std::string &home_dir );

    void run();

    inline const std::string & home_dir() const {
	return home_dir_;
    }

    const std::string & read_file(const std::string &path) const;

    void take_picture(std::vector<projected_star> &stars) const;
    void set_target(arith_t r, arith_t th, arith_t phi);
    void set_target(size_t proof_num, size_t nonce);
    vec3<arith_t> get_target() const;
    bool scan(size_t proof_num, std::vector<projected_star> &stars, size_t &nonce);

private:
    void do_accept();
    void setup_shutdown();
    void setup_acceptor(const std::string &address, const std::string &port);

    std::string home_dir_;

    boost::asio::io_service io_service_;
    boost::asio::signal_set signals_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;

    connections connections_;

    mutable std::unordered_map<std::string, std::string> file_cache_;

    void *observatory_;
};

}}

#endif
