//��֤���ļ���Ԥ���������ֻ������һ��
//��ֹ��ΰ���ͬһ��ͷ�ļ�
#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS//Ϊ��ʹ��inet_ntoa()����
	#define _CRT_SECURE_NO_WARNINGS //Ϊ��ʹ��scanf

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

	//��ʼ��socket
	void InitSocket() {
#ifdef _WIN32
		//����win sock2����
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);//����Windows socket����
#endif
		//����socket�׽���
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>�رվ�����...\n", _sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			printf("����,����socketʧ��...\n");
		}
		else {
			//printf("����<socket=%d>�ɹ�...\n", _sock);
		}
		
	}

	//���ӷ�����
	int Connect(const char* ip, unsigned short port) {
		if (INVALID_SOCKET == _sock) {
			InitSocket();
		}
		//���ӷ�����connect
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);

#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif
		//printf("<socket=%d>׼������<%s:%d>...\n", _sock, ip, port);

		int ret = connect(_sock, (sockaddr*)& _sin, sizeof(sockaddr_in));
		if (SOCKET_ERROR == ret) {
			printf("<socket=%d>ERROR:��������<%s:%d>ʧ��...\n", _sock, ip, port);
		}
		else {
			_isConnect = true;
			//printf("<socket=%d>��������<%s:%d>�ɹ�...\n", _sock, ip, port);
		}
		return ret;
	}

	//�ر�socket
	void Close() {
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			//�ر�win sock2����
			closesocket(_sock);
			WSACleanup();//�ر�socket����
#else
			close(_sock);
#endif
			_sock = INVALID_SOCKET;
		}
		_isConnect = false;
	}

	//����������Ϣ
	int _nCount = 0;
	bool OnRun() {
		if (isRun()) {
			fd_set fdReads;
			FD_ZERO(&fdReads);
			FD_SET(_sock, &fdReads);
			timeval t = { 0, 0 };
			int ret = select(_sock + 1, &fdReads, NULL, NULL, &t);
			if (ret < 0) {
				printf("<socket=%d>select�������1..\n", _sock);
				Close();
				return false;
			}
			//�ж�socket�Ƿ��ڼ�����
			if (FD_ISSET(_sock, &fdReads)) {
				FD_CLR(_sock, &fdReads);

				if (-1 == RecvData(_sock)) {
					printf("<socket=%d>select�������2..\n", _sock);
					Close(); 
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//�Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET && _isConnect;
	}

#ifndef RECV_BUFF_SIZE
	//��������С��Ԫ��С
#define RECV_BUFF_SIZE 10240
#endif
	//���ջ�����
	char _szRecv[RECV_BUFF_SIZE] = {};
	//��2������ ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE * 5] = {};
	//��Ϣ������������β��λ��
	int _lastPost = 0;
	//�������� ����ճ�� ��ְ�
	int RecvData(SOCKET _cSock) {

		//���շ���˷��͵�����
		int nLen = (int)recv(_cSock, _szRecv, RECV_BUFF_SIZE, 0);
		//printf("nLen = %d\n", nLen);
		if (nLen <= 0) {
			printf("<socket=%d>��������Ͽ����ӣ��������...\n", _sock);

			return -1;
		}
		//����ȡ�������ݿ�������Ϣ������
		memcpy(_szMsgBuf + _lastPost, _szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		_lastPost += nLen;
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		while (_lastPost >= sizeof(DataHeader)) {
		//��ʱ��֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (_lastPost >= header->dataLength) {
				//��Ϣ������ʣ��δ�������ݳ���
				int nSize = _lastPost - header->dataLength;
				//������������
				OnNetMsg(header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize);
				_lastPost = nSize;
			}
			else {
				//��Ϣ������ʣ��δ�������ݲ���һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
	virtual void OnNetMsg(DataHeader* header) {
		switch (header->cmd) {
		case CMD_LOGIN_RESULT: {
			LoginResult* loginResult = (LoginResult*)header;
			//printf("<socket=%d>���յ��������Ϣ��CMD_LOGIN_RESULT,���ݳ��ȣ�%d\n", _sock, loginResult->dataLength);

		};
							   break;
		case CMD_LOGOUT_RESULT: {
			LogoutResult* logoutResult = (LogoutResult*)header;
			//printf("<socket=%d>���յ��������Ϣ��CMD_LOGOUT_RESULT,���ݳ��ȣ�%d\n", _sock, logoutResult->dataLength);

		};
								break;
		case CMD_NEW_USER_JOIN: {
			NewUserJoin* newUserJoin = (NewUserJoin*)header;
			//printf("<socket=%d>���յ��������Ϣ��CMD_NEW_USER_JOIN,���ݳ��ȣ�%d\n", _sock, newUserJoin->dataLength);

		}
								break;
		case CMD_ERROR: {
			printf("<socket=%d>���յ��������Ϣ��CMD_ERROR,���ݳ��ȣ�%d\n", _sock, header->dataLength);
		}
						break;
		default: {
			printf("<socket=%d>���յ�δ������Ϣ,���ݳ��ȣ�%d\n", _sock, header->dataLength);

		}
		}
	}

	//��������
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