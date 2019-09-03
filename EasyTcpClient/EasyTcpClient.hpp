//保证此文件在预处理过程中只被包含一次
//防止多次包含同一个头文件
#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
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
	#define SOCKET_ERROR             (-1)
#endif

#include <stdio.h>
#include "MessageHeader.hpp"

class EasyTcpClient {

	SOCKET _sock;
	bool _isConnect;
public:

	EasyTcpClient() {
		_sock = INVALID_SOCKET;
		_isConnect = false;
	}

	virtual ~EasyTcpClient() {
		Close();
	}

	//初始化socket
	void InitSocket() {
#ifdef _WIN32
		//启动win sock2环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);//启动Windows socket环境
#endif
		//创建socket套接字
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>关闭旧连接...\n", _sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			printf("错误,建立socket失败...\n");
		}
		else {
			//printf("建立<socket=%d>成功...\n", _sock);
		}
		
	}

	//连接服务器
	int Connect(const char* ip, unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			InitSocket();
		}
		//连接服务器connect
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);

#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		//printf("<socket=%d>准备连接<%s:%d>...\n", _sock, ip, port);

		int ret = connect(_sock, (sockaddr*)& _sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret) {
			printf("<socket=%d>ERROR:建立连接<%s:%d>失败...\n", _sock, ip, port);
		}
		else {
			_isConnect = true;
			//printf("<socket=%d>建立连接<%s:%d>成功...\n", _sock, ip, port);
		}
		return ret;
	}

	//关闭socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			//关闭win sock2环境
			closesocket(_sock);
			WSACleanup();//关闭socket环境
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
		_isConnect = false;
	}

	//处理网络消息
	int _nCount = 0;
	bool OnRun() {
		if (isRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);
			timeval t = { 0, 0 };
			int ret = select(_sock + 1, &fdReads, NULL, NULL, &t);
			if (ret < 0) {
				printf("<socket=%d>select任务结束1..\n", _sock);
				Close();
				return false;
			}
			//判断socket是否在集合中
			if (FD_ISSET(_sock, &fdReads)) {
				FD_CLR(_sock, &fdReads);

				if (-1 == RecvData(_sock)) {
					printf("<socket=%d>select任务结束2..\n", _sock);
					Close(); 
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//是否工作中
	bool isRun() {
		return _sock != INVALID_SOCKET && _isConnect;
	}

#ifndef RECV_BUFF_SIZE
	//缓冲区最小单元大小
#define RECV_BUFF_SIZE 10240
#endif
	//接收缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};
	//第2缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE * 5] = {};
	//消息缓冲区的数据尾部位置
	int _lastPost = 0;
	//接收数据 处理粘包 拆分包
	int RecvData(SOCKET _cSock) {

		//接收服务端发送的数据
		int nLen = (int)recv(_cSock, _szRecv, RECV_BUFF_SIZE, 0);
		//printf("nLen = %d\n", nLen);
		if (nLen <= 0) {
			printf("<socket=%d>与服务器断开连接，任务结束...\n", _sock);

			return -1;
		}
		//将收取到的数据拷贝到消息缓冲区
		memcpy(_szMsgBuf + _lastPost, _szRecv, nLen);
		//消息缓冲区的数据尾部位置后移
		_lastPost += nLen;
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		while (_lastPost >= sizeof(DataHeader)) {
		//这时就知道当前消息的长度
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//判断消息缓冲区的数据长度大于消息长度
			if (_lastPost >= header->dataLength) {
				//消息缓冲区剩余未处理数据长度
				int nSize = _lastPost - header->dataLength;
				//处理网络数据
				OnNetMsg(header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize);
				_lastPost = nSize;
			}
			else {
				//消息缓冲区剩余未处理数据不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	virtual void OnNetMsg(DataHeader* header) {
		switch (header->cmd) {
		case CMD_LOGIN_RESULT: {
			LoginResult* loginResult = (LoginResult*)header;
			//printf("<socket=%d>接收到服务端消息：CMD_LOGIN_RESULT,数据长度：%d\n", _sock, loginResult->dataLength);

		};
							   break;
		case CMD_LOGOUT_RESULT: {
			LogoutResult* logoutResult = (LogoutResult*)header;
			//printf("<socket=%d>接收到服务端消息：CMD_LOGOUT_RESULT,数据长度：%d\n", _sock, logoutResult->dataLength);

		};
								break;
		case CMD_NEW_USER_JOIN: {
			NewUserJoin* newUserJoin = (NewUserJoin*)header;
			//printf("<socket=%d>接收到服务端消息：CMD_NEW_USER_JOIN,数据长度：%d\n", _sock, newUserJoin->dataLength);

		}
								break;
		case CMD_ERROR: {
			printf("<socket=%d>接收到服务端消息：CMD_ERROR,数据长度：%d\n", _sock, header->dataLength);
		}
						break;
		default: {
			printf("<socket=%d>接收到未定义消息,数据长度：%d\n", _sock, header->dataLength);

		}
		}
	}

	//发送数据
	int SendData(DataHeader* header, int nLen) {
		int ret = SOCKET_ERROR;
		if (isRun() && header) {
			ret = send(_sock, (const char*)header, nLen, 0);
			if (ret == SOCKET_ERROR) {
				Close();
			}
		}
		return ret;
	}

private:

};

#endif