#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE 10240
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS//为了使用inet_ntoa()函数
	#define _CRT_SECURE_NO_WARNINGS //为了使用scanf

	#include <Windows.h>
	#include <WinSock2.h>
	#pragma comment(lib, "ws2_32.lib")
#else
	#include<unistd.h> //uni std
	#include<arpa/inet.h>
	#include<string.h>

	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

#include <stdio.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"

//缓冲区最小单元大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif

//客户端数据类型
class ClientSocket {
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET) {
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPost = 0;
	}
	SOCKET sockfd() {
		return _sockfd;
	}
	char* msgBuf() {
		return _szMsgBuf;
	}
	int getLastPos() {
		return _lastPost;
	}
	void setLastPos(int pos) {
		_lastPost = pos;
	}

	//发送指定socket数据
	int SendData(DataHeader* header) {
		if (header) {
			return send(_sockfd, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
private:
	SOCKET _sockfd;
	//第2缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE * 5];
	//消息缓冲区的数据尾部位置
	int _lastPost;
};
//网络事件接口
class INetEvent {
public:
	//纯虚函数 客户端离开事件
	virtual void OnLeave(ClientSocket* pClient) = 0;
	//纯虚函数 客户端加入事件
	virtual void OnNetJoin(ClientSocket* pClient) = 0;
	//客户端消息事件
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) = 0;
private:

};

class CellServer {
public:
	CellServer(SOCKET sock = INVALID_SOCKET) {
		_sock = sock;
		_pNetEvent = nullptr;
	}
	~CellServer() {
		Close();
		_sock = INVALID_SOCKET;
	}

	void setNetEventObj(INetEvent* event) {
		_pNetEvent = event;
	}

	//关闭socket
	void Close() {
		//关闭socket
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			closesocket(_sock);
#else
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
#endif
			_clients.clear();
		}
	}

	//处理网络消息
	bool OnRun() {
		while (isRun()) {
			if (_clientsBuff.size() > 0){
				//从缓冲队列中取出客户数据
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) {
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//如果没有客户连接就跳过
			if (_clients.empty()) {
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//伯克利socket
			fd_set fdRead;
			//清空描述符集
			FD_ZERO(&fdRead);
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				if (maxSock < _clients[n]->sockfd()) maxSock = _clients[n]->sockfd();
				FD_SET(_clients[n]->sockfd(), &fdRead);
			}

			//nfds是指fd_set集合中所有socket值的最大值加一，指的是范围而不是数量
			//在Windows中这个参数可以为0
			//在这里设置为非阻塞，表示查询相应时间后返回
			//select函数的最后一个参数设置为NULL即为阻塞
			int ret = select(maxSock + 1, &fdRead, 0, 0, nullptr);
			if (ret < 0) {
				printf("select任务结束。\n");
				Close();
				return false;
			}
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				if (FD_ISSET(_clients[n]->sockfd(), &fdRead)) {
					if (-1 == RecvData(_clients[n])) {
						auto iter = _clients.begin() + n;
						if (iter != _clients.end()) {
							if (_pNetEvent)_pNetEvent->OnLeave(_clients[n]);
							delete _clients[n];
							_clients.erase(iter);
						}
					}
				}
			}
		}
		return false;
	}

	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	//缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};

	//接收数据 处理粘包 拆分包
	int RecvData(ClientSocket* pClient) {

		//接收客户端发送的数据
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		//printf("nLen = %d\n", nLen);
		if (nLen <= 0) {
			//printf("客户端<socket=%d>已退出，任务结束...\n", pClient->sockfd());
			return -1;
		}
		//LoginResult ret;
		//SendData(pClient->sockfd(), &ret);

		//将收取到的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLastPos() + nLen);
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (pClient->getLastPos() >= sizeof(DataHeader)) {
			//这时就知道当前消息的长度
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->getLastPos() >= header->dataLength) {
				//消息缓冲区剩余未处理数据长度
				int nSize = pClient->getLastPos() - header->dataLength;
				//处理网络数据
				OnNetMsg(pClient, header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				pClient->setLastPos(nSize);
			}
			else {
				//消息缓冲区剩余未处理数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) {
		//回调 统计数据包
		_pNetEvent->OnNetMsg(pClient, header);
	}

	void addClient(ClientSocket* pClient) {
		std::lock_guard<std::mutex> lock(_mutex);
		//_mutex.lock();
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}

	void Start() {
		_pThread = std::thread(std::mem_fn(&CellServer::OnRun), this);
	}

	size_t getClientCount() {
		return _clients.size() + _clientsBuff.size();
	}
private:

	SOCKET _sock;
	//客户端队列
	std::vector<ClientSocket*> _clients;
	//客户端连接缓冲区
	std::vector<ClientSocket*> _clientsBuff;
	//缓冲队列的锁
	std::mutex _mutex;
	std::thread _pThread;
	//网络事件对象
	INetEvent* _pNetEvent;
};

class EasyTcpServer : public INetEvent {
private:

	SOCKET _sock;
	//消息处理对象 每个对象对应一个线程
	std::vector<CellServer*> _cellServers;
	//每秒计时
	CELLTimestamp _tTime;
protected:
	//收到数据包数量
	std::atomic_int _recvCount;
	//客户端计数
	std::atomic_int _clientCount;
public:
	EasyTcpServer() {
		_sock = INVALID_SOCKET;
		_recvCount = 0;
		_clientCount = 0;
	}
	virtual ~EasyTcpServer() {
		Close();
	}
	//初始化socket
	SOCKET InitSocket() {
#ifdef _WIN32
		//启动win sock2环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);//启动Windows socket环境
#endif
		//创建socket套接字
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			printf("ERROR:建立socket失败...\n");
		}
		else {
			printf("建立<socket=%d>成功...\n", (int)_sock);
		}
		return _sock;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port) {
		if (_sock == INVALID_SOCKET) {
			InitSocket();
		}
		//bind绑定客户端连接的网络端口
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);//转换成网络字节序
#ifdef _WIN32
	//或者inet_addr("127.0.0.1");//转换成网络字节序
		if (ip) {
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip) {
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif 
		int ret = bind(_sock, (sockaddr*)& _sin, sizeof(_sin));
		if (SOCKET_ERROR == ret) {
			printf("ERROR：绑定用于接收客户端连接的端口<%d>失败...\n", port);
		}
		else {
			printf("绑定端口<%d>成功...\n", port);
		}
		return ret;
	}

	//监听端口号
	int Listen(int n) {
		//listen监听套接字，
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret) {
			printf("<socket=%d>ERROR：监听套接字失败...\n", (int)_sock);
		}
		else {
			printf("<socket=%d>监听套接字成功...\n", (int)_sock);
		}
		return ret;
	}

	//接收客户端连接
	int Accept() {
		sockaddr_in clientAddr = {};//客户端的socket
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)& clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)& clientAddr, (socklen_t*)& nAddrLen);
#endif
		if (cSock == INVALID_SOCKET) {
			printf("<socket=%d>ERROR:无效客户端socket...\n", (int)_sock);
		}
		else {
			//将新客户端分配给客户数量最少的cellserver
			addClientToCellServer(new ClientSocket(cSock));
			//printf("<socket=%d>新客户端<%d>：socket = %d,IP = %s \n", (int)_sock, _clients.size(), (int)cSock, inet_ntoa(clientAddr.sin_addr));
		}
		return cSock;
	}
	
	void addClientToCellServer(ClientSocket* pClient) {
		//查找客户数量最少的cellserver消息处理对象
		auto pMinServer = _cellServers[0];
		for (auto pCellServer : _cellServers) {
			if (pMinServer->getClientCount() > pCellServer->getClientCount()) {
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
		OnNetJoin(pClient);
	}

	void Start(int nCellServer) {
		for (int i = 0; i < nCellServer; i++) {
			auto ser = new CellServer(_sock);
			_cellServers.push_back(ser);
			//注册网络事件对象
			ser->setNetEventObj(this);
			//启动消息处理线程
			ser->Start();
		}
	}

	//关闭socket
	void Close() {
		//关闭socket
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();//关闭socket环境
#else
			close(_sock);
#endif
		}
	}


	//处理网络消息
	bool OnRun() {
		if (isRun()) {
			time4msg();
			//伯克利socket
			fd_set fdRead;
			//清空描述符集
			FD_ZERO(&fdRead);

			FD_SET(_sock, &fdRead);

			//nfds是指fd_set集合中所有socket值的最大值加一，指的是范围而不是数量
			//在Windows中这个参数可以为0
			//在这里设置为非阻塞，表示查询相应时间后返回
			//select函数的最后一个参数设置为NULL即为阻塞
			timeval t = { 0, 10 };
			int ret = select(_sock + 1, &fdRead, 0, 0, &t);
			if (ret < 0) {
				printf("Accept select任务结束。\n");
				Close();
				return false;
			}

			if (FD_ISSET(_sock, &fdRead)) {
				FD_CLR(_sock, &fdRead);
				//accept等待客户端连接
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}

	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	//计算并输出每秒收到的网络消息
	void time4msg() {
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0) {
			printf("threads = %d, time = %lf, <socket = %d>, clients = %d, recvCount = %d\n", _cellServers.size(), t1, (int)_sock, (int)_clientCount, (int)(_recvCount / t1));
			_tTime.update();
			_recvCount = 0;
		}
	}
	

	virtual void OnLeave(ClientSocket* pClient) {
		_clientCount--;
	}
	virtual void OnNetJoin(ClientSocket* pClient) {
		_clientCount++;
	}
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)
	{
		_recvCount++;
		switch (header->cmd) {
		case CMD_LOGIN: {

			Login* login = (Login*)header;
			//printf("接收到客户端<socket = %d>的命令：CMD_LOGIN,数据长度：%d,username = %s,PassWord = %s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确
			LoginResult ret;
			pClient->SendData(&ret);
		};
						break;
		case CMD_LOGOUT: {

			Logout* logout = (Logout*)header;
			//printf("接收到客户端<socket = %d>的命令：CMD_LOGOUT,数据长度：%d,username = %s \n", cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确
			//LogoutResult ret;
			//SendData(cSock, &ret);
		};
						 break;
		default: {
			printf("<socket=%d>接收到未定义消息,数据长度：%d\n", pClient->sockfd(), header->dataLength);

			//DataHeader ret;
			//SendData(cSock, &ret);
		}
				 break;
		}
	}
};

#endif