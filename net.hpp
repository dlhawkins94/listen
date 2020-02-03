#pragma once

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

using namespace std;

const string socket_path = "/tmp/shizuka_listen.socket";

/*
 * The network IO for the listen system is very simple -- it exists just to
 * pass short commands to the program. The server listens on a unix socket.
 * When listen is not performing some special function (i.e., it's called as
 * a daemon to perform ASR) this server will be created.
 *
 * When listen is running as a secondary process to modify the ASR parameters
 * somehow, it can pass messages to the main process using send_cmd.
 */

class server {
  int sockfd, serv_len;
  struct sockaddr_un serv_addr;

public:
  server();
  ~server();
  string await_cmd();
};

bool send_cmd(string cmd);
