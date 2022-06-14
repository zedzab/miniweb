#pragma once
#include "Macros.h"

#include <functional>
#include <map>
#include <vector>
class EventLoop;
class Socket;
class Acceptor;
class Connection;
class ThreadPool;
class Server {
 private:
  EventLoop *main_reactor_;
  Acceptor *acceptor_;
  std::map<int, Connection *> connections_;
  std::vector<EventLoop *> sub_reactors_;
  ThreadPool *thread_pool_;
  //std::function<void(Connection *)> on_connect_callback_;

 public:
  explicit Server(EventLoop *loop);
  ~Server();

  std::function<void(Connection *)> on_connect_callback_;
  DISALLOW_COPY_AND_MOVE(Server);

  void NewConnection(Socket *sock);
  void DeleteConnection(Socket *sock);
  //void OnConnect(std::function<void(Connection *)> fn);
  void do_http_request(Connection *conn);
  void do_http_response(int client_sock,const char *path);
  int headers(int client_sock,FILE *resource);
  void cat(int client_sock,FILE *resource);
  void inner_error(int client_sock);
  int get_line(int sock,char *buf,int size);
  void not_found(int client_sock);
};
