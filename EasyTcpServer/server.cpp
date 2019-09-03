#include "EasyTcpServer.hpp"
#include <thread>

bool g_bRun = true;
void cmdThread() {
	while (true) {
		char cmdBuf[256] = {};
		scanf("%s", cmdBuf);
		if (0 == strcmp(cmdBuf, "exit")) {
			g_bRun = false;
			printf("退出cmdThread...\n");
			return;
		}
		else {
			printf("不支持的命令...\n");
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
		printf("client = %d离开\n", (int)pClient->sockfd());
		
	}
	virtual void OnNetJoin(ClientSocket* pClient) {
		_clientCount++;
		printf("client = %d加入\n", (int)pClient->sockfd());
	}
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header){
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
int main() {


	MyServer server;
	server.InitSocket();
	server.Bind(nullptr, 4567);
	server.Listen(5);
	server.Start(4);

	//启动线程
	std::thread t1(cmdThread);
	t1.detach();

	while (g_bRun) {

		server.OnRun();
		//printf("空闲时间处理其他业务..\n");
	}

	server.Close();
	printf("服务器任务结束...\n");
	getchar();
	return 0;

}

