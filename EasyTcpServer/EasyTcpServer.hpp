#ifndef _EasyTcpServer_hpp_
#define _EasyTcpServer_hpp_

#ifdef _WIN32
	#define FD_SETSIZE 10240
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
	#define SOCKET_ERROR            (-1)
#endif

#include <stdio.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"

//��������С��Ԫ��С
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240
#endif

//�ͻ�����������
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

	//����ָ��socket����
	int SendData(DataHeader* header) {
		if (header) {
			return send(_sockfd, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}
private:
	SOCKET _sockfd;
	//��2������ ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE * 5];
	//��Ϣ������������β��λ��
	int _lastPost;
};
//�����¼��ӿ�
class INetEvent {
public:
	//���麯�� �ͻ����뿪�¼�
	virtual void OnLeave(ClientSocket* pClient) = 0;
	//���麯�� �ͻ��˼����¼�
	virtual void OnNetJoin(ClientSocket* pClient) = 0;
	//�ͻ�����Ϣ�¼�
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

	//�ر�socket
	void Close() {
		//�ر�socket
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

	//����������Ϣ
	bool OnRun() {
		while (isRun()) {
			if (_clientsBuff.size() > 0){
				//�ӻ��������ȡ���ͻ�����
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff) {
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}
			//���û�пͻ����Ӿ�����
			if (_clients.empty()) {
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
			//������socket
			fd_set fdRead;
			//�����������
			FD_ZERO(&fdRead);
			SOCKET maxSock = _clients[0]->sockfd();
			for (int n = (int)_clients.size() - 1; n >= 0; n--) {
				if (maxSock < _clients[n]->sockfd()) maxSock = _clients[n]->sockfd();
				FD_SET(_clients[n]->sockfd(), &fdRead);
			}

			//nfds��ָfd_set����������socketֵ�����ֵ��һ��ָ���Ƿ�Χ����������
			//��Windows�������������Ϊ0
			//����������Ϊ����������ʾ��ѯ��Ӧʱ��󷵻�
			//select���������һ����������ΪNULL��Ϊ����
			int ret = select(maxSock + 1, &fdRead, 0, 0, nullptr);
			if (ret < 0) {
				printf("select���������\n");
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

	//�Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	//������
	char _szRecv[RECV_BUFF_SIZE] = {};

	//�������� ����ճ�� ��ְ�
	int RecvData(ClientSocket* pClient) {

		//���տͻ��˷��͵�����
		int nLen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		//printf("nLen = %d\n", nLen);
		if (nLen <= 0) {
			//printf("�ͻ���<socket=%d>���˳����������...\n", pClient->sockfd());
			return -1;
		}
		//LoginResult ret;
		//SendData(pClient->sockfd(), &ret);

		//����ȡ�������ݿ�������Ϣ������
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nLen);
		//��Ϣ������������β��λ�ú���
		pClient->setLastPos(pClient->getLastPos() + nLen);
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		while (pClient->getLastPos() >= sizeof(DataHeader)) {
			//��ʱ��֪����ǰ��Ϣ�ĳ���
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (pClient->getLastPos() >= header->dataLength) {
				//��Ϣ������ʣ��δ�������ݳ���
				int nSize = pClient->getLastPos() - header->dataLength;
				//������������
				OnNetMsg(pClient, header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				pClient->setLastPos(nSize);
			}
			else {
				//��Ϣ������ʣ��δ�������ݲ���һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header) {
		//�ص� ͳ�����ݰ�
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
	//�ͻ��˶���
	std::vector<ClientSocket*> _clients;
	//�ͻ������ӻ�����
	std::vector<ClientSocket*> _clientsBuff;
	//������е���
	std::mutex _mutex;
	std::thread _pThread;
	//�����¼�����
	INetEvent* _pNetEvent;
};

class EasyTcpServer : public INetEvent {
private:

	SOCKET _sock;
	//��Ϣ������� ÿ�������Ӧһ���߳�
	std::vector<CellServer*> _cellServers;
	//ÿ���ʱ
	CELLTimestamp _tTime;
protected:
	//�յ����ݰ�����
	std::atomic_int _recvCount;
	//�ͻ��˼���
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
	//��ʼ��socket
	SOCKET InitSocket() {
#ifdef _WIN32
		//����win sock2����
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);//����Windows socket����
#endif
		//����socket�׽���
		if (INVALID_SOCKET != _sock) {
			printf("<socket=%d>�رվ�����...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock) {
			printf("ERROR:����socketʧ��...\n");
		}
		else {
			printf("����<socket=%d>�ɹ�...\n", (int)_sock);
		}
		return _sock;
	}

	//��IP�Ͷ˿ں�
	int Bind(const char* ip, unsigned short port) {
		if (_sock == INVALID_SOCKET) {
			InitSocket();
		}
		//bind�󶨿ͻ������ӵ�����˿�
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);//ת���������ֽ���
#ifdef _WIN32
	//����inet_addr("127.0.0.1");//ת���������ֽ���
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
			printf("ERROR�������ڽ��տͻ������ӵĶ˿�<%d>ʧ��...\n", port);
		}
		else {
			printf("�󶨶˿�<%d>�ɹ�...\n", port);
		}
		return ret;
	}

	//�����˿ں�
	int Listen(int n) {
		//listen�����׽��֣�
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret) {
			printf("<socket=%d>ERROR�������׽���ʧ��...\n", (int)_sock);
		}
		else {
			printf("<socket=%d>�����׽��ֳɹ�...\n", (int)_sock);
		}
		return ret;
	}

	//���տͻ�������
	int Accept() {
		sockaddr_in clientAddr = {};//�ͻ��˵�socket
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;
#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)& clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)& clientAddr, (socklen_t*)& nAddrLen);
#endif
		if (cSock == INVALID_SOCKET) {
			printf("<socket=%d>ERROR:��Ч�ͻ���socket...\n", (int)_sock);
		}
		else {
			//���¿ͻ��˷�����ͻ��������ٵ�cellserver
			addClientToCellServer(new ClientSocket(cSock));
			//printf("<socket=%d>�¿ͻ���<%d>��socket = %d,IP = %s \n", (int)_sock, _clients.size(), (int)cSock, inet_ntoa(clientAddr.sin_addr));
		}
		return cSock;
	}
	
	void addClientToCellServer(ClientSocket* pClient) {
		//���ҿͻ��������ٵ�cellserver��Ϣ�������
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
			//ע�������¼�����
			ser->setNetEventObj(this);
			//������Ϣ�����߳�
			ser->Start();
		}
	}

	//�ر�socket
	void Close() {
		//�ر�socket
		if (_sock != INVALID_SOCKET) {
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();//�ر�socket����
#else
			close(_sock);
#endif
		}
	}


	//����������Ϣ
	bool OnRun() {
		if (isRun()) {
			time4msg();
			//������socket
			fd_set fdRead;
			//�����������
			FD_ZERO(&fdRead);

			FD_SET(_sock, &fdRead);

			//nfds��ָfd_set����������socketֵ�����ֵ��һ��ָ���Ƿ�Χ����������
			//��Windows�������������Ϊ0
			//����������Ϊ����������ʾ��ѯ��Ӧʱ��󷵻�
			//select���������һ����������ΪNULL��Ϊ����
			timeval t = { 0, 10 };
			int ret = select(_sock + 1, &fdRead, 0, 0, &t);
			if (ret < 0) {
				printf("Accept select���������\n");
				Close();
				return false;
			}

			if (FD_ISSET(_sock, &fdRead)) {
				FD_CLR(_sock, &fdRead);
				//accept�ȴ��ͻ�������
				Accept();
				return true;
			}
			return true;
		}
		return false;
	}

	//�Ƿ�����
	bool isRun() {
		return _sock != INVALID_SOCKET;
	}

	//���㲢���ÿ���յ���������Ϣ
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
			//printf("���յ��ͻ���<socket = %d>�����CMD_LOGIN,���ݳ��ȣ�%d,username = %s,PassWord = %s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//�����ж��û������Ƿ���ȷ
			LoginResult ret;
			pClient->SendData(&ret);
		};
						break;
		case CMD_LOGOUT: {

			Logout* logout = (Logout*)header;
			//printf("���յ��ͻ���<socket = %d>�����CMD_LOGOUT,���ݳ��ȣ�%d,username = %s \n", cSock, logout->dataLength, logout->userName);
			//�����ж��û������Ƿ���ȷ
			//LogoutResult ret;
			//SendData(cSock, &ret);
		};
						 break;
		default: {
			printf("<socket=%d>���յ�δ������Ϣ,���ݳ��ȣ�%d\n", pClient->sockfd(), header->dataLength);

			//DataHeader ret;
			//SendData(cSock, &ret);
		}
				 break;
		}
	}
};

#endif