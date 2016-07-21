#include "http_connection.hpp"

#include <boost/asio/buffers_iterator.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <regex>
#include <boost/log/utility/manipulators/dump.hpp>


namespace eiptnd {

http_connection::http_connection(boost::shared_ptr<connection> connection)
  : log_(boost::log::keywords::channel = "http-connection@" +
        boost::lexical_cast<std::string>(connection->remote_endpoint()))
  , conn_(boost::move(connection))
{
}

template <typename Iterator>
bool parse_request(Iterator const& first, Iterator const& last,
                   std::string & mtd, std::string & url, std::string & ver)
{
  /*static const std::regex request_regex("([^\\s]+)\\s+"
                                        "([^\\s]+)\\s+"
                                        "HTTP/([\\d\\.]+)");
  typename std::match_results<Iterator> request_match;
  if (std::regex_match(first, last, request_match, request_regex)) {
    mtd = request_match[1];
    url = request_match[2];
    ver = request_match[3];
    return true;
  }
  return false;*/

  std::string delim(" ");
  auto mtd_last = std::search(first, last, std::begin(delim), std::end(delim));
  if (mtd_last == last) {
    return false;
  }
  auto url_first = std::next(mtd_last, 1);
  auto url_last = std::search(url_first, last, std::begin(delim), std::end(delim));
  if (url_last == last) {
    return false;
  }

  auto ver_first = std::next(mtd_last, 1);
  if (ver_first == last) {
    return false;
  }

  mtd.assign(first, mtd_last);
  url.assign(url_first, url_last);
  ver.assign(ver_first, last);

  return true;
}

void http_connection::send_file(std::string const& url)
{
  BOOST_LOG_SEV(log_, logging::trace)
    << "send_file(): " << url;

  auto found = url.find_first_of("?");
  if (std::string::npos == found) {
    found = url.find_first_of("#");
  }

  auto first = std::begin(url);
  auto last = (std::string::npos == found) ? std::end(url)
                                           : std::next(first, +found);

  // TODO: Add encoding support T_T
  std::string loc(first, last);

  if (loc == "/") {
    loc = "/index.html";
  }

  //boost::filesystem::path path(loc);
  boost::filesystem::path path(conn_->get_webroot());
  path /= boost::filesystem::path(loc);

  BOOST_LOG_SEV(log_, logging::trace)
    << "Converted path: " << path;

  if (!path.has_filename()) {
    make_simple_answer(204, "No Content", "Is not a file");
    return;
  }

  if (!boost::filesystem::exists(path)) {
    make_simple_answer(404, "Not Found", "Sorry :(");
    return;
  }

#ifdef ENABLE_SEGMENTED_TRANSFER
  auto f = boost::make_shared<std::ifstream>();
  f->open(path.string(), std::ios::binary);
  if (f->is_open()) {
    auto buf = boost::make_shared<std::ifstream>();
    auto cb = [f](){

    }
  }
#else
  std::ifstream f;
  f.open(path.string(), std::ios::binary);
  if (f.is_open()) {
    std::string content;
    f.seekg(0, std::ios::end);
    content.reserve(f.tellg());
    f.seekg(0, std::ios::beg);
    content.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    make_simple_answer(200, "OK", content);
  }
#endif
  else {
    make_simple_answer(500, "Internal Error", "Whoops!");
  }
}

void http_connection::make_simple_answer(unsigned short code,
                                         std::string const& repl,
                                         std::string const& body)
{
  BOOST_LOG_SEV(log_, logging::trace)
    << "Answer: " << code << " " << repl;

  std::ostringstream ss;
  ss << "HTTP/1.0 " << code << " " << repl << "\r\nConnection: Closed\r\n";
  if (body.size()) {
    ss << "Content-Length: " << body.size() << "\r\n"
          "Content-Type: text/html\r\n"
          "\r\n"
       << body;
  }
  else {
    ss << "\r\n";
  }
  auto buf = boost::make_shared<std::string>(ss.str());

  conn_->do_write_cb(boost::asio::buffer(*buf), [buf](){});
}

/*Date: Mon, 27 Jul 2009 12:28:53 GMT
Server: Apache/2.2.14 (Win32)
Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT
Content-Length: 88
Content-Type: text/html
Connection: Closed*/

template <typename Iterator>
bool http_connection::process_request(Iterator & first, Iterator const& last)
{
  auto iter = first;

  /*static const std::regex line_regex("\r\n");
  std::match_results<Iterator> line_match;*/
  std::string mtd, url, ver;

  //BOOST_LOG_SEV(log_, logging::flood)
  //  << "line_match.size() == " << line_match.size();

  // Iterate over every line
  std::string delim("\r\n");
  for (std::size_t i = 0; iter != last; ++i) {
    auto found = std::search(iter, last, std::begin(delim), std::end(delim));
    std::string s(iter, found);
    //if(std::regex_search(iter, last, line_match, line_regex)) {
      //auto const& s = line_match.prefix();
      //auto const& found = s.second;
      //auto size = std::distance(iter, s.second);

      // Empty line is the marker of the end of headers
      //if (line_match.prefix().second == iter) {
      if (iter == found) {
        auto size = std::distance(found, last) - delim.size();
        if (size > 0) {
          BOOST_LOG_SEV(log_, logging::trace)
            << "Request has body of " << size << " bytes";
          make_simple_answer(400, "Bad Request",
                             "I don't understand what you want");
        }
        else {
          send_file(url);
        }
        break;
      }

      // First line is request string, while other is fields
      if (i == 0) {
        if (parse_request(iter, found, mtd, url, ver)) {
          BOOST_LOG_SEV(log_, logging::trace) << "MTD: " << mtd;
          BOOST_LOG_SEV(log_, logging::trace) << "URL: " << url;
          BOOST_LOG_SEV(log_, logging::trace) << "VER: " << ver;
        }
        else {
          BOOST_LOG_SEV(log_, logging::error)
            << "Parsing request failed: " << s;
          return false;
        }
      }
      else {
        static const std::regex field_regex("([^:\\s]+)\\s*:\\s*(.*)");
        std::match_results<Iterator> field_match;
        if (std::regex_match(iter, found, field_match, field_regex)) {
            //fields.emplace(std::string(field_match[1].first, field_match[1].second),
            //               std::string(field_match[2].first, field_match[2].second));
        }
        else {
          BOOST_LOG_SEV(log_, logging::error)
            << "Parsing field failed: " << s;
          return false;
        }
      }
      //iter = line_match.suffix().first;
    //}
    iter = std::next(found, delim.size());
  }

  first = iter;
  return true;
}

void http_connection::handle_start()
{
  conn_->do_read_until(in_buf_, "\r\n\r\n");
}

void http_connection::handle_read(std::size_t bytes_transferred)
{
  BOOST_LOG_SEV(log_, logging::trace)
    << "handle_read(): bytes=" << bytes_transferred;

  auto bufs = in_buf_.data();
  auto first = boost::asio::buffers_begin(bufs);
  auto last = boost::asio::buffers_end(bufs);

  std::vector<char> data(first, last);
  BOOST_LOG_SEV(log_, logging::flood)
    << "do_read_until(): " << boost::log::dump(data.data(), data.size());

#ifdef ENABLE_HTTP_11_SUPPORT
  if (process_request(first, last)) {
    in_buf_.consume(std::distance(first, last));
    handle_start();
  }
  else {
    conn_->close();
    conn_.reset();
  }
#else
  if (!process_request(first, last)) {
    make_simple_answer(500, "Internal Error", "Whaat?");
  }
  conn_->close();
  conn_.reset();
#endif
}

void http_connection::handle_write()
{

}

}
