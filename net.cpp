#include "net.hpp"

server::server() {
  if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    cerr << "Failed to create socket" << endl;
    exit(1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, socket_path.c_str());
  serv_len = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

  // make sure socket file isn't left over
  struct stat test;
  if (!stat(socket_path.c_str(), &test))
    unlink(socket_path.c_str());
  
  if (bind(sockfd, (struct sockaddr*) &serv_addr, serv_len) < 0) {
    cerr << "Failed to bind socket" << endl;
    exit(1);
  }

  listen(sockfd, 4);
}

server::~server() {
  close(sockfd);
  unlink(socket_path.c_str());
}

string server::await_cmd() {
  struct sockaddr_un cli_addr;
  socklen_t cli_len = sizeof(cli_addr);
  int clifd = accept(sockfd, (struct sockaddr*) &cli_addr, &cli_len);
  if (clifd < 0) {
    cerr << "Failed to accept client cmd" << endl;
    exit(1);
  }

  char buff[256];
  memset(buff, 0, sizeof(buff));
  int n = read(clifd, buff, sizeof(buff));
  write(clifd, "pong", 4);
  close(clifd);

  return string(buff);
}

bool send_cmd(string cmd) {
  struct sockaddr_un serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, socket_path.c_str());
  int serv_len = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    cerr << "Failed to create client socket" << endl;
    return false;
  }

  if (connect(sockfd, (struct sockaddr*) &serv_addr, serv_len) < 0) {
    cerr << "Failed to connect to server" << endl;
    return false;
  }

  char buff[256];
  memset(buff, 0, sizeof(buff));
  strcpy(buff, cmd.c_str());
  write(sockfd, buff, strlen(buff));
  
  memset(buff, 0, sizeof(buff));
  int n = read(sockfd, buff, sizeof(buff));
  close(sockfd);

  return true;
}
