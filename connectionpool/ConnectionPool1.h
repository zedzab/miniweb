#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string.h>
#include <string>
#include "MysqlConn.h"
class ConnectionPool{
public:
	static ConnectionPool* getConnectPool();
	ConnectionPool(const ConnectionPool& obj)=delete;//析构函数
	ConnectionPool& operator=(const ConnectionPool& obj)=delete;//复制操作符重载的等号操作符删除掉
	std::shared_ptr<MysqlConn> getConnection();//数据库连接获取
	~ConnectionPool();
private:
	ConnectionPool();
	bool parseJsonFile();
	void produceConnection();
	void recycleConnection();
	void addConnection();

	std::string m_ip;
	std::string m_user;
	std::string m_passwd;
	std::string m_dbName;
	unsigned short m_port;
	int m_minSize;
	int m_maxSize;
	int m_timeout;
	int m_maxIdleTime;
	std::queue<MysqlConn*> m_connectionQ;
	std::mutex m_mutexQ;
	std::condition_variable m_cond;
};
//g++ main.cpp MysqlConn.cpp MysqlConn.h ConnectionPool.cpp ConnectionPool.h  -o main.o -lmysqlclient
