#include "Server.h"

#include <unistd.h>

#include <functional>

#include "Acceptor.h"
#include "Connection.h"
#include "EventLoop.h"
#include "Socket.h"
#include "ThreadPool.h"
#include "util.h"
#include "Buffer.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include <string>
#include <iostream>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

static int debug=1;

Server::Server(EventLoop *loop) : main_reactor_(loop), acceptor_(nullptr), thread_pool_(nullptr) {
  acceptor_ = new Acceptor(main_reactor_);
  std::function<void(Socket *)> cb = std::bind(&Server::NewConnection, this, std::placeholders::_1);
  acceptor_->SetNewConnectionCallback(cb);

  int size = static_cast<int>(std::thread::hardware_concurrency());
  thread_pool_ = new ThreadPool(size);
  for (int i = 0; i < size; ++i) {
    sub_reactors_.push_back(new EventLoop());
  }

  for (int i = 0; i < size; ++i) {
    std::function<void()> sub_loop = std::bind(&EventLoop::Loop, sub_reactors_[i]);
    thread_pool_->Add(std::move(sub_loop));
  }
}

Server::~Server() {
  delete acceptor_;
  delete thread_pool_;
}

void Server::NewConnection(Socket *sock) {
  ErrorIf(sock->GetFd() == -1, "new connection error");
  uint64_t random = sock->GetFd() % sub_reactors_.size();
  Connection *conn = new Connection(sub_reactors_[random], sock);
  std::function<void(Socket *)> cb = std::bind(&Server::DeleteConnection, this, std::placeholders::_1);
  conn->SetDeleteConnectionCallback(cb);
  std::function<void(Connection*)> on_connect_callback_=std::bind(&Server::do_http_request,this,std::placeholders::_1);
  conn->SetOnConnectCallback(on_connect_callback_);
  connections_[sock->GetFd()] = conn;
}

void Server::DeleteConnection(Socket *sock) {
  int sockfd = sock->GetFd();
  auto it = connections_.find(sockfd);
  if (it != connections_.end()) {
    Connection *conn = connections_[sockfd];
    connections_.erase(sockfd);
    delete conn;
    conn = nullptr;
  }
}

//void Server::OnConnect(std::function<void(Connection *)> fn) { on_connect_callback_ = std::move(fn); }//服务器逻辑

void Server::do_http_request(Connection *conn){
    //conn->Read();
    if (conn->GetState() == Connection::State::Closed) {
      conn->Close();
      return;
    }
    //char *buf=conn->ReadBuffer();
    //conn->Write();
	int len=0;
	char buf[256];
	char method[64];
	char url[256];
	char path[256];
	struct stat st;//这个结构体是用来描述一个linux系统文件系统中的文件属性的结构。第一个参数都是文件的路径，第二个参数是struct stat的指针。返回值为0，表示成功执行
	//读取客户端发送的http请求
	Socket * client=conn->GetSocket();
	int client_sock=client->GetFd();	
	len=get_line(client_sock,buf,sizeof(buf));
	if(len>0){//读取成功
		//1.读取请求行
		int i=0,j=0;
		while(!isspace(buf[j])&&(i<sizeof(method)-1)){//是否是白空格
			method[i]=buf[j];
			i++;j++;
		}
		method[i]='\0';
		if(debug)printf("request method:%s\n",method);
		if(strncasecmp(method,"GET",i)==0){//只处理get请求,不区分大小写
			if(debug) printf("request method=GET\n");
		//获取url;
			while(isspace(buf[j++]));//跳过白空格
			i=0;
			while(!isspace(buf[j])&&(i<sizeof(url)-1)){
				url[i]=buf[j];
				i++;
				j++;
			}//url请求内容
			url[i]='\0';
			if(debug) printf("url:%s\n",url);//拿到url
			//继续读取http头部
			do{
				len=get_line(client_sock,buf,sizeof(buf));
				if(debug) printf("read:%s\n",buf);
			//这里只是打印
			}while(len>0);
		//定位服务器本地的html文件，执行http响应
			//处理url中的？，问号之前的是文件名
			char *pos=strchr(url,'?');
			if(pos){
				*pos='\0';
				printf("real url:%s\n",url);
			}
			sprintf(path,"./html_docs/%s",url);//定位到html
			if(debug) printf("path:%s\n",path);
			//执行http响应
			//判断文件是否存在，如果存在就响应200 OK，同时发送相应的html文件，如果不存在，就响应404 NOT FOUND。
			if(stat(path,&st)==-1){//文件不存在或者出错，返回-1
				fprintf(stderr,"stat %s failed.reason:%s\n",path,strerror(errno));
				not_found(client_sock);//待实现的函数
			}else{//文件存在
				if(S_ISDIR(st.st_mode)){//是目录
					strcat(path,"/log.html");//在当前路径下寻找文件
				}
				std::cout<<"path is"<<path<<std::endl;
				do_http_response(client_sock,path);
			}
		}else{//非get请求，读取http头部，并响应客户端501
			fprintf(stderr,"warning! other request [%s]\n",method);
			do{
				len=get_line(client_sock,buf,sizeof(buf));
				if(debug) printf("read:%s\n",buf);
			//这里只是打印
			}while(len>0);
			//unimplemented(client_sock);501后面实现
			
		}
		
	}else{//请求格式有问题
	
	}
}

void Server::do_http_response(int client_sock,const char *path){
	int ret=0;
	FILE *resource=NULL;
	resource=fopen(path,"r");
	if(resource==NULL){
		not_found(client_sock);
		return;
	}
	//1.发送http头部
	ret=headers(client_sock,resource);
	//2.发送http body.
	if(!ret){
		cat(client_sock,resource);
	}
	fclose(resource);
}
//成功为0，失败为-1
int Server::headers(int client_sock,FILE *resource){
	char buf[1024]={0};
	struct stat st;
	int fileid=0;
	char tmp[64];
	strcpy(buf,"HTTP/1.0 200 OK\r\n");
	strcat(buf,"Server:Martin Server\r\n");
	strcat(buf,"Content-Type: text/html\r\n");
	strcat(buf,"Connection:Close\r\n");

	fileid=fileno(resource);//文件id
	if(fstat(fileid,&st)==-1){//-1表示打开文件失败，几乎不会发生这种情况
		inner_error(client_sock);
		return -1;
	}
	snprintf(tmp,64,"Content-Length:%1d\r\n\r\n",st.st_size);
	strcat(buf,tmp);
	if(debug) fprintf(stdout,"header:%s\n",buf);
	if(send(client_sock,buf,strlen(buf),0)<0){//发送失败
		fprintf(stderr,"send failed:data:%s,reason:%s\n",buf,strerror(errno));
		return -1;
	}
	return 0;
}
//将html文件内容按行发送给客户端,resource文件句柄
void Server::cat(int client_sock,FILE *resource){
	char buf[1024];
	fgets(buf,sizeof(buf),resource);
	//判断是否到达文件的尾部
	while(!feof(resource)){
		int len=write(client_sock,buf,strlen(buf));
		if(len<0){//送的过程中出现问题
			fprintf(stderr,"send body error. reason:%s\n",strerror(errno));//正式的话写到日志
			break;
		}
		if(debug) fprintf(stdout,"%s",buf);
		fgets(buf,sizeof(buf),resource);
	}
}

void Server::inner_error(int client_sock){
	const char *reply="HTTP/1.0 500 Internal Server Error\r\n\
Content-Type:text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Inner Error</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>服务器内部出错.\r\n\
</BODY>\r\n\
</HTML>";
	int len=write(client_sock,reply,strlen(reply));

	if(debug)fprintf(stdout,reply);
	if(len<=0){
		fprintf(stderr,"send reply failed.reason:%s\n",strerror(errno));
	}
}

void do_http_response1(int client_sock){
	const char * main_header="HTTP/1.0 200 OK\r\nServer:Martin Server\r\nContent-Type: text/html\r\nConnection:Close\r\n";//固定头部
	const char * welcome_content="<!DOCTYPE html>\n\
<html>\n\
    <head>\n\
        <meta charset=\"UTF-8\">\n\
        <title>Sign in</title>\n\
    </head>\n\
    <body>\n\
<br/>\n\
<br/>\n\
    <div align=\"center\"><font size=\"5\"> <strong>登录</strong></font></div>\n\
    <br/>\n\
        <div class=\"login\">\n\
                <form action=\"2CGISQL.cgi\" method=\"post\">\n\
                        <div align=\"center\"><input type=\"text\" name=\"user\" placeholder=\"用户名\" required=\"required\"></div><br/>\n\
                        <div align=\"center\"><input type=\"password\" name=\"password\" placeholder=\"登录密码\" required=\"required\"></div><br/>\n\
                        <div align=\"center\"><button type=\"submit\">确定</button></div>\n\
                </form>\n\
        </div>\n\
    </body>\n\
</html>";//欢迎页面
//1.送main_header 2.生成contentlength并发送 3.发送html文件内容
//1.送main_header
	int len=write(client_sock,main_header,strlen(main_header));
	if(debug) fprintf(stdout,"...do http response ...\n");
	if(debug) fprintf(stdout,"write[%d]:%s",len,main_header);
//2.生成contentlength并发送
	char send_buf[64];	
	int wc_len=strlen(welcome_content);
	len=snprintf(send_buf,64,"Content-Length:%d\r\n\r\n",wc_len);
	len=write(client_sock,send_buf,len);

	if(debug)fprintf(stdout,"write[%d]:%s",len,send_buf);

	len=write(client_sock,welcome_content,wc_len);
	if(debug) fprintf(stdout,"write[%d]:%s",len,welcome_content);
}

//返回值：-1表示读取出错，大于0表示成功读取一行
int Server::get_line(int sock,char *buf,int size){
	int count=0;//读了多少字符
	char ch='\0';//字符串读取完毕
	int len=0;//读取成功还是失败
	while((count<size-1)&&ch!='\n'){
		len=read(sock,&ch,1);
		if(len==1){//成功
			if(ch=='\r'){
				continue;
			}else if(ch=='\n'){
				buf[count]='\0';
				break;
			}
			//这里正常字符
			buf[count]=ch;
			count++;
		}else if(len==-1){
			perror("read failed");
			break;
		}else{//read返回0,客户端关闭socket连接
			fprintf(stderr,"client_close.\n");
			break;
		}
	}
	if(count>=0)buf[count]='\0';
	return count;
}

void Server::not_found(int client_sock){
	const char *reply ="HTTP/1.0 404 NOT FOUND\r\n\
Content-Type:text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html;charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>NOT FOUND</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>文件不存在！\r\n\
</BODY>\r\n\
<HTML>";
	int len=write(client_sock,reply,strlen(reply));
	if(debug) fprintf(stdout,reply);
	
	if(len<=0){
		fprintf(stderr,"send_reply failed. reason:%s\n",strerror(errno));
	}
}
