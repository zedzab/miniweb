#include "ConnectionPool.h"
#include <json/json.h>
#include <fstream>
#include <thread>
using namespace Json;
ConnectionPool *ConnectionPool::getConnectPool(){
	static ConnectionPool pool;
	return &pool;
}

bool ConnectionPool::parseJsonFile(){
	ifstream ifs("dbconf.json");//解析这个json
	Reader rd;//添加reader对象
	Value root;
	rd.parse(ifs,root);//函数调用成功后，root里就有数据
	if(root.isObject()){//判断是不是json对象
		m_ip=root["ip"].asString();
		m_port=root["port"].asInt();
		m_user=root["user"].asString();
		m_passwd=root["passwd"].asString();
		m_dbName=root["dbName"].asString();
		m_minSize=root["minSize"].asInt();
		m_maxSize=root["maxSize"].asInt();
		m_maxIdleTime=root["maxIdleTime"].asInt();
		m_timeout=root["timeout"].asInt();
		return true;
	}
	return false;
}// -L /usr/local/lib /usr/local/lib/libjsoncpp.a

void ConnectionPool::addConnection(){
	MysqlConn* conn=new MysqlConn;
	conn->connect(m_user,m_passwd,m_dbName,m_ip,m_port);
	conn->refreshAliveTime();
	m_connectionQ.push(conn);
}

ConnectionPool::~ConnectionPool(){
	while(!m_connectionQ.empty()){
		MysqlConn* conn=m_connectionQ.front();
		m_connectionQ.pop();
		delete conn;
	}
}

ConnectionPool::ConnectionPool(){
	if(!parseJsonFile()){
		return;
	}
	for(int i=0;i<m_minSize;i++){
		addConnection();
	}
	//检测连接池里连接够不够用
	thread producer(&ConnectionPool::produceConnection,this);//生产连接
	thread recycler(&ConnectionPool::recycleConnection,this);//销毁连接
	producer.detach();
	recycler.detach();
}

void ConnectionPool::produceConnection(){//生产连接
	while(true){
		unique_lock<mutex> locker(m_mutexQ);
		while(m_connectionQ.size()>=m_minSize){
			m_cond.wait(locker);
		}
		addConnection();
		m_cond.notify_all();
	}
}

void ConnectionPool::recycleConnection(){
	while(true){
		this_thread::sleep_for(chrono::milliseconds(500));
		lock_guard<mutex> locker(m_mutexQ);
		while(m_connectionQ.size()>m_minSize){
			MysqlConn* conn=m_connectionQ.front();
			if(conn->getAliveTime()>=m_maxIdleTime)
			{
				m_connectionQ.pop();
				delete conn;
			}
			else{
			break;
			}
		}
	}
}

shared_ptr<MysqlConn> ConnectionPool::getConnection(){
	unique_lock<mutex> locker(m_mutexQ);
	while(m_connectionQ.empty()){
		if(cv_status::timeout==m_cond.wait_for(locker,chrono::milliseconds(m_timeout))){
			if(m_connectionQ.empty()){continue;}//return nullptr
		}
	}
	shared_ptr<MysqlConn> connptr(m_connectionQ.front(),[this](MysqlConn *conn){
		lock_guard<mutex> locker(m_mutexQ);		
		conn->refreshAliveTime();//更新连接
		m_connectionQ.push(conn);//指定删除函数
		});
	m_connectionQ.pop();
	m_cond.notify_all();
	return connptr;
}
