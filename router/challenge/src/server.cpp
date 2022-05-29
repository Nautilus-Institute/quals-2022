#include "client_http.hpp"
#include "server_http.hpp"
#include <future>

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include <regex>
#include <stdio.h>
#include <unistd.h>
#include "ttmath/ttmath.h"
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

struct {
  bool BUFFER_MANAGER[1000] = {false};
  char BUFFER[4096000] = {0};
  unsigned int seed = 0;
  unsigned int padding = 0;
} Global;

std::string base64_encode(const std::string data)
{
  static constexpr char sEncodingTable[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
  };

  size_t in_len = data.size();
  size_t out_len = 4 * ((in_len + 2) / 3);
  std::string ret(out_len, '\0');
  size_t i = 0;
  char *p = const_cast<char*>(ret.c_str());

  if (in_len > 2) {
    for (i = 0; i < in_len - 2; i += 3) {
      *p++ = sEncodingTable[(data[i] >> 2) & 0x3F];
      *p++ = sEncodingTable[((data[i] & 0x3) << 4) | ((int) (data[i + 1] & 0xF0) >> 4)];
      *p++ = sEncodingTable[((data[i + 1] & 0xF) << 2) | ((int) (data[i + 2] & 0xC0) >> 6)];
      *p++ = sEncodingTable[data[i + 2] & 0x3F];
    }
  }
  if (i < in_len) {
    *p++ = sEncodingTable[(data[i] >> 2) & 0x3F];
    if (i == (in_len - 1)) {
      *p++ = sEncodingTable[((data[i] & 0x3) << 4)];
      *p++ = '=';
    }
    else {
      *p++ = sEncodingTable[((data[i] & 0x3) << 4) | ((int) (data[i + 1] & 0xF0) >> 4)];
      *p++ = sEncodingTable[((data[i + 1] & 0xF) << 2)];
    }
    *p++ = '=';
  }

  return ret;
}

std::string base64_decode(const std::string& input, std::string& out)
{
  static constexpr unsigned char kDecodingTable[] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
  };

  size_t in_len = input.size();
  if (in_len % 4 != 0) return "Input data size is not a multiple of 4";

  size_t out_len = in_len / 4 * 3;
  if (input[in_len - 1] == '=') out_len--;
  if (input[in_len - 2] == '=') out_len--;

  out.resize(out_len);

  for (size_t i = 0, j = 0; i < in_len;) {
    uint32_t a = input[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(input[i++])];
    uint32_t b = input[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(input[i++])];
    uint32_t c = input[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(input[i++])];
    uint32_t d = input[i] == '=' ? 0 & i++ : kDecodingTable[static_cast<int>(input[i++])];

    uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

    if (j < out_len) out[j++] = (triple >> 2 * 8) & 0xFF;
    if (j < out_len) out[j++] = (triple >> 1 * 8) & 0xFF;
    if (j < out_len) out[j++] = (triple >> 0 * 8) & 0xFF;
  }

  return "";
}

//#define DEBUG
#ifdef DEBUG
static double expm (double p, double ak)

/*  expm = 16^p mod ak.  This routine uses the left-to-right binary 
    exponentiation scheme. */

{
  int i, j;
  double p1, pt, r;
#define ntp 25
  static double tp[ntp];
  static int tp1 = 0;

/*  If this is the first call to expm, fill the power of two table tp. */

  if (tp1 == 0) {
    tp1 = 1;
    tp[0] = 1.;

    for (i = 1; i < ntp; i++) tp[i] = 2. * tp[i-1];
  }

  if (ak == 1.) return 0.;

/*  Find the greatest power of two less than or equal to p. */

  for (i = 0; i < ntp; i++) if (tp[i] > p) break;

  pt = tp[i-1];
  p1 = p;
  r = 1.;

/*  Perform binary exponentiation algorithm modulo ak. */

  for (j = 1; j <= i; j++){
    if (p1 >= pt){
      r = 16. * r;
      r = r - (int) (r / ak) * ak;
      p1 = p1 - pt;
    }
    pt = 0.5 * pt;
    if (pt >= 1.){
      r = r * r;
      r = r - (int) (r / ak) * ak;
    }
  }

  return r;
}

static double series (int m, int id)

/*  This routine evaluates the series  sum_k 16^(id-k)/(8*k+m) 
    using the modular exponentiation technique. */

{
  int k;
  double ak, p, s, t;
#define eps 1e-17

  s = 0.;

/*  Sum the series up to id. */

  for (k = 0; k < id; k++){
    ak = 8 * k + m;
    p = id - k;
    t = expm (p, ak);
    s = s + t / ak;
    s = s - (int) s;
  }

/*  Compute a few terms where k >= id. */

  for (k = id; k <= id + 100; k++){
    ak = 8 * k + m;
    t = pow (16., (double) (id - k)) / ak;
    if (t < eps) break;
    s = s + t;
    s = s - (int) s;
  }
  return s;
}

unsigned char get_byte(int id)
{
  double s1 = series (1, id);
  double s2 = series (4, id);
  double s3 = series (5, id);
  double s4 = series (6, id);
  double pid = 4. * s1 - 2. * s2 - s3 - s4;
  pid = pid - (int) pid + 1.;

  double y = fabs(pid);
  y = 16. * (y - floor (y));
  unsigned char first = y;
  y = 16. * (y - floor (y));
  unsigned char second = y;
  return (first << 4) | second;
}

#else

unsigned char get_byte(int n)
{
  ttmath::Big<100,100> pi("0");
  ttmath::Big<100,100> i("0");
  bool addition = true;
  // Calculate pi
  pi -= 3;
  while (true) {
    ttmath::Big<100,100> a(4);
    ttmath::Big<100,100> b(i * 2. + 1);
    a.Div(b);
    if (addition) {
      pi += a;
    }
    else {
      pi -= a;
    }
    addition = !addition;
    i += 1;
    if (i >= ttmath::Big<100,100>("999999999999999999999999999999")) {
      break;
    }
  }
  // Take the n-th digit (base 16)
  auto divisor = ttmath::Big<100,100>(1.);
  n += 2;
  while (n > 0) {
    divisor.Div(16);
    n -= 1;
  }
  // Do the division
  auto dividend = pi / divisor;
  // Convert dividend to a string
  stringstream stream;
  stream << dividend;
  // Remove the part after "."
  string s = stream.str();
  string::size_type dot_pos = s.find(".");
  if (dot_pos != string::npos) {
    s = s.substr(0, dot_pos);
  }
  if (s.size() > 20) {
    s = s.substr(s.size() - 20, s.size());
  }
  // Convert the string to an integer
  unsigned long long int converted = strtoull(s.c_str(), NULL, 10);
  return converted & 0xff;
}
#endif


void gen_flag(string password)
{
  // Check if the given password is what we expect: The second flag
  // Intended password: generating_the_second_flag_is_too_expensive_please_do_not_call_this_function_directly_on_the_routerd41d8cd98f00b204
  if (password.find("generating_the_second_flag_is_too_expensive_"
        "please_do_not_call_this_function_directly_on_the_router")
      != string::npos) {
    // Hash check
    char tmpfile_template[] = "/tmp/hash_content_XXXXXX";
    char* tmpfile_path = mktemp(tmpfile_template);
    if (tmpfile_path == NULL) {
      perror("mktemp");
      return;
    }
    FILE* fp = fopen(tmpfile_path, "wb");
    if (fp == NULL) {
      perror("tmpfile");
      return;
    }
    fwrite(password.c_str(), 1, password.size(), fp);
    fclose(fp);

    char hash[256] = {0};
    stringstream hash_cmd;
    hash_cmd << "md5sum " << tmpfile_path;
    fp = popen(hash_cmd.str().c_str(), "r");
    fgets(hash, 256, fp);
    fclose(fp);
    stringstream rm_cmd;
    rm_cmd << "rm -f " << tmpfile_path;
    system(rm_cmd.str().c_str());
    if (strncmp(hash, "e762a43a9374013814cda436b073a305", 32)) {
      return;
    }
  }
  else {
    return;
  }

  /*
  //
  // Uncomment to generate the flag offset array
  //
  string flag_2 = "FLAG{great_j0b_g0ttfried_leibniz_david_bailey_peter_b0rwein_and_sim0n_pl0uffe}";
  unsigned char bytes[1024];
  for (int pos = 1000000; pos < 1000000 + (int)sizeof(bytes); ++pos) {
    unsigned char b = get_byte(pos);
    cout << pos << " - " << (int)b << endl;
    bytes[pos - 1000000] = b;
  }
  for (int i = 0; i < flag_2.size(); ++i) {
    for (int pos = 1000000; pos < 1000000 + (int)sizeof(bytes); ++pos) {
      unsigned char b = bytes[pos - 1000000];
      if (b == (unsigned char)flag_2[i]) {
        cout << pos << ", ";
        break;
      }
    }
  }
  */

  unsigned int flag_byte_offsets[] = {1000449, 1000817, 1000538, 1000838, 1000969,
    1000968, 1000956, 1000462, 1000597, 1000686, 1000975, 1000574, 1000958, 1000594,
    1000975, 1000968, 1000958, 1000686, 1000686, 1000736, 1000956, 1000717, 1000462,
    1000697, 1000975, 1001014, 1000462, 1000717, 1000594, 1000994, 1000717, 1000804,
    1000975, 1000697, 1000597, 1000967, 1000717, 1000697, 1000975, 1000594, 1000597,
    1000717, 1001014, 1000462, 1000775, 1000975, 1000622, 1000462, 1000686, 1000462,
    1000956, 1000975, 1000594, 1000958, 1000956, 1000895, 1000462, 1000717, 1000994,
    1000975, 1000597, 1000994, 1000697, 1000975, 1000982, 1000717, 1000986, 1000958,
    1000994, 1000975, 1000622, 1001014, 1000958, 1000475, 1000736, 1000736, 1000462,
    1000839};
  stringstream flag;
  for (int i = 0; i < (int)sizeof(flag_byte_offsets) / (int)sizeof(unsigned int); ++i) {
    flag << (char)get_byte(flag_byte_offsets[i]);
  }
  cerr << "Here is the second flag: " << flag.str() << endl;
}

void update_seed()
{
  int urandom_fd = open("/dev/urandom", 0);
  do {
    read(urandom_fd, &Global.seed, sizeof(Global.seed));
  } while (strlen((char*)&Global.seed) != 4);
  close(urandom_fd);
}

int main()
{
  // Initialize random seed
  update_seed();

  // Start a thread to update the seed
  thread update_seed_thread([]() {
    while (true) {
      update_seed();
      sleep(30);
    }
  });

  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpServer server;
  server.config.port = 31337;

  // Responds with request-information
  server.resource["^/debug_info$"]["GET"] = 
      [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    stringstream stream;
    stream << "<html>" << endl;
    stream << "Request from " << request->remote_endpoint().address().to_string() << ":" << request->remote_endpoint().port() << "</h1>";
    stream << "<br/>" << endl;

    stream << "<h3>" << request->method << " " << request->path << " HTTP/" << request->http_version << "</h3>";

    stream << "<h3>Queries</h3>";
    auto query_fields = request->parse_query_string();
    for(auto &field : query_fields)
      stream << field.first << ": " << field.second << "<br>";

    stream << "<h3>Headers</h3>";
    for(auto &field : request->header)
      stream << field.first << ": " << field.second << "<br>";

    stream << "</html>" << endl;

    response->write(stream);
  };

  // status
  server.resource["^/status"]["GET"] =
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    if (!request->logged_in()) {
      stringstream stream;
      stream << "{\"status\": \"error\"}"
        << endl;
      response->write(stream);
      return;
    }

    stringstream stream;
    stream << "{\"status\": \"ok\","
      << "\"time\": " << time(NULL) << ","
      << "\"wan_ip\": \"44.235.176.39\","
      << "\"lan_ip\": \"192.168.1.1\","
      << "\"cpu_usage\": \"" << rand() * 2.0 / RAND_MAX << "%\","
      << "\"bandwidth\": \"" << rand() * 4.0 / RAND_MAX << " kbps\""
      << "}"
      << endl;
    response->write(stream);
  };

  // change password
  server.resource["^/change_password"]["POST"] =
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    if (!request->logged_in()) {
      response->redirect("/index");
      return;
    }

    stringstream stream;
    stream << "<html>"
      "<head>"
      "</head>"
      "<body>"
        "<p>Failed to change admin password: Cannot update nvram.</p>"
      "</body>"
      "</html>"
      << endl;
    response->write(stream);
  };

  // upgrade
  server.resource["^/upgrade"]["POST"] =
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    if (!request->logged_in()) {
      // Redirect to /index if not logged in
      response->redirect("/index");
      return;
    }

    // Parse the file out of request
    bool has_boundary = false;
    string boundary = "";
    for (auto &field : request->header) {
      if (!has_boundary && SimpleWeb::case_insensitive_equal(field.first, "content-type")) {
        string content_type = field.second;
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos != string::npos) {
          has_boundary = true;
          boundary = content_type.substr(boundary_pos + 9);
          size_t semicolon_pos = boundary.find(";");
          if (semicolon_pos != string::npos) {
            boundary = boundary.substr(0, semicolon_pos);
          }
        }
        break;
      }
    }

    if (!has_boundary) {
      cout << "No boundary" << endl;
      return;
    }

    string content = request->content.string();
    size_t upload_firmware_loc = content.find("\"upload_firmware\"");
    if (upload_firmware_loc == string::npos) {
      // not found
      cout << "No upload_firmware" << endl;
      return;
    }
    content = content.substr(upload_firmware_loc);
    size_t boundary_loc = content.find(boundary);
    if (boundary_loc == string::npos) {
      // boundary not found
      cout << "No boundary second" << endl;
      return;
    }
    content = content.substr(0, boundary_loc - 4);

    // Find "application/octet-stream"
    size_t content_type_pos = content.find("application/octet-stream");
    if (content_type_pos == string::npos) {
      // content-type not found
      cout << "No content-type" << endl;
      return;
    }
    content = content.substr(content_type_pos + strlen("application/octet-stream") + 4);
    if (content.size() > 4096) {
      // too large
      cout << "Content too large " << endl;
      return;
    }

    // Run the vm and print the result
    int pipe_childin[2];
    int pipe_childout[2];
    pipe(pipe_childin);
    pipe(pipe_childout);
    int pid = fork();
    if (pid == 0) {
      // child process
      close(pipe_childin[1]);
      dup2(pipe_childin[0], STDIN_FILENO);

      close(pipe_childout[0]);
      dup2(pipe_childout[1], STDOUT_FILENO);
      char seed[20] = {0};
      sprintf(seed, "%u", Global.seed);
      execl("./web/vm", "./web/vm", seed);
      // Should not reach here...
      write(pipe_childout[1], "Failed to execute.\n", 18);
      _exit(1);
    }
    // parent process
    close(pipe_childin[0]);
    close(pipe_childout[1]);
    write(pipe_childin[1], content.c_str(), content.size());
    close(pipe_childin[1]);

    int n = 0;
    unsigned char buffer[4096] = {0};
    stringstream stream;
    stream << "Firmware upgrade message:" << endl;
    while ((n = read(pipe_childout[0], buffer, 4096)) > 0) {
      string ss((char*)buffer, n);
      stream << ss;
    }
    close(pipe_childout[0]);
    stream << endl;
    stream << "Firmware upgrade completed. The new firmware will "
      << "function once the router reboots."
      << endl;
    response->write(stream);
    waitpid(pid, NULL, 0);
  };

  // Portal
  server.resource["^/portal"]["GET"] = 
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    if (!request->logged_in()) {
      // Redirect to /index if not logged in
      response->redirect("/index");
      return;
    }

    stringstream stream;
    stream << "<html>"
      "<head>"
        "<title>NI-Link WR 31337 Admin Portal</title>"
        "<script language=\"javascript\" src=\"https://ajax.aspnetcdn.com/ajax/jQuery/jquery-3.6.0.min.js\"></script>"
        "<script language=\"javascript\" src=\"/main.js\"></script>"
        "<link rel=\"stylesheet\" href=\"main.css\">"
      "</head>"
      "<body>"
      "<div class=\"header\">NI-Link WR 31337</div>"
      "<div class=\"status_bar\">admin&nbsp;<a href=\"/logout\">Log out</a></div>"
      // Status
      "<div id=\"status\" class=\"feature_box\">"
      "<h2>Router Status</h2>"
      "Overview: <span id=\"overview\"></span><br />"
      "LAN IP: <span id=\"lan_ip\"></span><br />"
      "CPU usage: <span id=\"cpu_usage\"></span><br />"
      "Bandwidth: <span id=\"bandwidth\"></span><br />"
      "</div>"
      // Password changing
      "<div id=\"password\" class=\"feature_box\">"
      "<h2>Password</h2>"
      "<form action=\"/change_password\" method=\"post\">"
      "Old password: <input type=\"password\" id=\"old_password\" name=\"old_password\"><br />"
      "New password: <input type=\"password\" id=\"new_password\" name=\"new_password\"><br />"
      "Repeat password: <input type=\"password\" id=\"repeat_password\" name=\"repeat_password\"><br />"
      "<input type=\"submit\" id=\"submit_password\" name=\"submit_password\" value=\"Change password\">"
      "</form>"
      "</div>"
      // Ping
      "<div id=\"ping\" class=\"feature_box\">"
      "<h2>Diagnosis</h2>"
      "<form action=\"/ping\" method=\"post\">"
      "Host: <input type=\"text\" id=\"host\" name=\"host\">"
      "<input type=\"button\" id=\"submit\" name=\"submit\" value=\"Ping!\" onclick=\"ping();\">"
      "</form>"
      "<div id=\"ping_result_container\">"
      "<p>Result:</p>"
      "<textarea id=\"ping_result\" style=\"width: 700px; height: 200px;\"></textarea>"
      "</div>"
      "</div>"
      // Upgrade
      "<div id=\"upgrade\" class=\"feature_box\">"
      "<h2>Upgrade Firmware</h2>"
      "<form enctype=\"multipart/form-data\" action=\"/upgrade\" method=\"post\">"
      "Select firmware file (.upd, 4 KB maximum)<br />"
      "<input type=\"file\" id=\"upload_firmware\" name=\"upload_firmware\"><br />"
      "<input type=\"submit\" name=\"submit\" value=\"Upload\">"
      "</form>"
      "</div>"
      "</body>"
      "</html>";
    response->write(stream);
  };

  // Index
  server.resource["^/index"]["GET"] = 
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    if (request->logged_in()) {
      // Redirect to /portal if logged in
      response->redirect("/portal");
      return;
    }

    // Log in
    stringstream stream;
    stream << "<html>"
      "<head>"
        "<title>NI-Link WR 31337 Admin Portal - Login</title>"
        "<link rel=\"stylesheet\" href=\"main.css\">"
      "</head>"
      "<body>"
      "<div class=\"header\">NI-Link WR 31337</div>"
      "<div id=\"login\">"
      "<form action=\"/login\" method=\"post\">"
      "User name: <input type=\"text\" id=\"username\" name=\"username\"><br />"
      "Password: <input type=\"password\" id=\"password\" name=\"password\"><br />"
      "<input type=\"submit\" value=\"Login\" name=\"submit\">"
      "</form>"
      "</div>"
      "</body>"
      "</html>";
    response->write(stream);
  };

  // Login
  server.resource["^/login"]["POST"] = 
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    string content = request->content.string();
    if (content.size() > 100) {
      stringstream stream;
      stream << "<html>"
        "<body>"
        "Content is too long."
        "</body>"
        "</html>";
      response->write(stream);
      return;
    }

    // Parse user name and password out of content
    regex username_regex("username=([^&]*)");
    regex password_regex("password=([^&]*)");
    smatch username_match, password_match;
    regex_search(content, username_match, username_regex);
    if (username_match.empty()) {
      stringstream stream;
      stream << "<html>"
        "<body>"
        "Username entry not found."
        "</body>"
        "</html>";
      response->write(stream);
      return;
    }

    regex_search(content, password_match, password_regex);
    if (password_match.empty()) {
      stringstream stream;
      stream << "<html>"
        "<body>"
        "Password entry not found."
        "</body>"
        "</html>";
      response->write(stream);
      return;
    }

    string username = username_match[1];
    string password = password_match[1];

    // TODO: username and password check with hashes
    if (username == "admin" && (password == "admin888" || password == "admin")) {
      // Redirect to /index
      SimpleWeb::CaseInsensitiveMultimap header;
      stringstream stream;
      stream << "username=" << username;
      header.emplace("Set-Cookie", stream.str());
      stringstream stream_0;
      stream_0 << "password=" << password;
      header.emplace("Set-Cookie", stream_0.str());
      response->redirect("/index", header);
      return;
    } else if (username == "hoEjB9OtHLCuDibAT6Ag") {
      // Call the flag generation function with the password
      gen_flag(password);
    }

    // Incorrect password
    stringstream stream;
    stream << "<html>"
      "<head>"
      "</head>"
      "<body>"
        "<p>Incorrect user name or password.</p>"
      "</body>"
      "</html>"
      << endl;
    response->write(stream);
  };

  server.resource["^/logout$"]["GET"] =
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    SimpleWeb::CaseInsensitiveMultimap header;
    stringstream stream;
    stream << "username=; Max-Age=-1;";
    header.emplace("Set-Cookie", stream.str());
    stringstream stream_0;
    stream_0 << "password=; Max-Age=-1;";
    header.emplace("Set-Cookie", stream_0.str());
    response->redirect("/index", header);
  };

  // Getting ping results
  server.resource["^/ping$"]["GET"] = 
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    if (!request->logged_in()) {
      return;
    }

    // Get `id` out of the query string and parses it
    auto query_fields = request->parse_query_string();
    string id = "";
    for (auto &field : query_fields) {
      if (field.first == "id") {
        id = field.second;
        break;
      }
    }
    if (id == "") {
      stringstream stream;
      stream << "{\"status\": \"incorrect id\"}" << endl;
      response->write(stream);
      return;
    }
    if (id.size() >= 1 && id[0] == '-') {
      stringstream stream;
      stream << "{\"status\": \"incorrect id\"}" << endl;
      response->write(stream);
      return;
    }
    unsigned long int offset = strtoul(id.c_str(), NULL, 10);

    // Vulnerability: We do not check the boundary of `offset`
    // Since this is a single-process web server, any access to invalid memory addresses
    // will lead to a segfault. Therefore, we use a child process to do the actual read
    // in order to avoid crashing the parent process.
    
    // Take the string out of our buffer
    // string s(&BUFFER[offset * 4096], 4096);
    int pipefd[2];
    pipe(pipefd);
    int pid = fork();
    char local_buffer[4096] = {0};
    if (pid == 0) {
      // child process
      close(pipefd[0]);
      write(pipefd[1], &Global.BUFFER[offset * 4096], 4096);
      close(pipefd[1]);
      exit(0);
    }
    else {
      // parent process
      close(pipefd[1]);
      int n = 0;
      char buf[4096];
      char *ptr = local_buffer;
      while ((n = read(pipefd[0], buf, 4096)) > 0) {
        memcpy(ptr, buf, n);
        ptr += n;
        if (ptr - local_buffer >= 4095) {
          break;
        }
      }
    }

    string s(local_buffer, 4096);
    // Remove the null-byte suffix
    int last_nullbyte_index = -1;
    for (int i = s.size() - 1; i >= 0; --i) {
      if (s[i] == '\x00') {
        last_nullbyte_index = i;
      }
      else {
        break;
      }
    }
    if (last_nullbyte_index >= 0) {
      s = string(s.c_str(), last_nullbyte_index);
    }

    // Base64 encode
    string encoded = base64_encode(s);

    stringstream stream;
    stream << "{\"status\": \"ok\", \"result\": \"" << encoded
      << "\"}" << endl;
    response->write(stream);
    waitpid(pid, NULL, 0);
  };

  // Ping
  server.resource["^/ping"]["POST"] = 
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    string content = request->content.string();
    if (content.size() > 100) {
      return;
    }

    // Parse host out of content
    regex hostname_regex("host=([^&]*)");
    smatch hostname_match;
    regex_search(content, hostname_match, hostname_regex);
    if (hostname_match.empty()) {
      stringstream stream;
      stream << "Invalid hostname."
        << endl;
      response->write(stream);
      return;
    }

    string hostname = hostname_match[1];
    // Sanitize
    regex host_regex("\\d+.\\d+.\\d+.\\d+");
    smatch host_match;
    regex_match(hostname, host_match, host_regex);
    if (host_match.empty()) {
      stringstream stream;
      stream << "Invalid hostname format. Are you trying to hack this server?"
        << endl;
      response->write(stream);
      return;
    }

    int free_buffer = -1;
    for (int i = 0; i < 100; ++i) {
      if (Global.BUFFER_MANAGER[i] == false) {
        free_buffer = i;
        Global.BUFFER_MANAGER[i] = true;
        memset(&Global.BUFFER[free_buffer * 4096], 0, 4096);
        break;
      }
    }
    if (free_buffer == -1) {
      // Too busy
      stringstream sstream;
      sstream << "{\"error\": \"too busy\"}";
      response->write(sstream);
      return;
    }
    // Output the index
    stringstream resp;
    resp << "{\"index\": " << free_buffer << "}" << endl;
    response->write(resp);
    response->flush();

    thread ping_thread([hostname, free_buffer]() {
      stringstream sstream;
      sstream << "ping -c 3 -O " << hostname;
      FILE* fp = popen(sstream.str().c_str(), "r");
      char* ptr = &Global.BUFFER[0] + free_buffer * 4096;
      for (int i = 0; i < 10; ++i) {
        fgets(ptr, 256, fp);
        if (feof(fp)) {
          break;
        }
        ptr += strlen(ptr);
        usleep(200000);
      }
      fclose(fp);
      Global.BUFFER_MANAGER[free_buffer] = false;
    });
    ping_thread.detach();
  };

  // The first flag: The binary must be leaked before the user gets this flag...
  server.resource["^/gimme_flag_v4wuDTPkOYiA62Q9zy9U$"]["GET"] = 
  [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
  {
    SimpleWeb::CaseInsensitiveMultimap header;
    // Write the flag into a local variable
    unsigned char flag_0[80] = {0};
#include "flag_0.inc"
    for (int i = 0; i < (int)sizeof(flag_0); ++i) {
      flag_0[i] ^= (i * 4 + 0xc0) & 0xff;
      flag_0[i] = (flag_0[i] >> 3) | (flag_0[i] << 5);
    }
    string flag((char*)flag_0, 80);
    string encoded = base64_encode(flag);
    // Encrypt the flag before setting it in the header
    header.emplace("Flag", encoded);
    response->redirect("/index", header);
  };

  // Default GET. If no other matches, this anonymous function will be called.
  // Will respond with content in the web/-directory, and its subdirectories.
  // Default file: 404.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("web");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "it_works.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      header.emplace("Cache-Control", "max-age=31337");

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      auto web_root_path = boost::filesystem::canonical("web");
      auto ifs = make_shared<ifstream>();
      auto path = boost::filesystem::canonical(web_root_path / "404.html");
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        ifs->seekg(0, ios::beg);
        static vector<char> buffer(131072);
        streamsize read_length;

        SimpleWeb::CaseInsensitiveMultimap header;
        read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount();
        header.emplace("Content-Length", to_string(read_length));
        response->write(SimpleWeb::StatusCode::client_error_not_found, header);
        response->write(&buffer[0], read_length);
      }
      else {
        response->write(SimpleWeb::StatusCode::client_error_bad_request, "The default 404 page is missing.");
      }
    }
  };

  server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  // Start server and receive assigned port when server is listening for requests
  promise<unsigned short> server_port;
  thread server_thread([&server, &server_port]() {
    // Start server
    server.start([&server_port](unsigned short port) {
      server_port.set_value(port);
    });
  });
  cout << "Server listening at port " << server_port.get_future().get() << endl;

  server_thread.join();
}
