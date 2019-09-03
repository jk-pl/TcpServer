#include "EasyTcpServer.hpp"
#include <thread>

bool g_bRun = true;
void cmdThread() {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit")) {
			g_bRun = false;
			printf("�˳�cmdThread...\n");
			return;
		}
		else {
			printf("��֧�ֵ�����...\n");
		}
	}
}

class MyServer :public EasyTcpServer{
public:
	MyServer() {

	}
	~MyServer() {

	}
	virtual void OnLeave(ClientSocket* pClient) {
		_clientCount--;
		printf("client = %d�뿪\n", (int)pClient->sockfd());
		
	}
	virtual void OnNetJoin(ClientSocket* pClient) {
		_clientCount++;
		printf("client = %d����\n", (int)pClient->sockfd());
	}
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header){
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
int main() {


	MyServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);
	server.Start(4);

	//�����߳�
	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun) {

		server.OnRun();
		//printf("����ʱ�䴦������ҵ��..\n");
	}

	server.Close();
	printf("�������������...\n");
	getchar();
	return 0;

}

