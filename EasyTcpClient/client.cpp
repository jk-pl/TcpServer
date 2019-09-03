#include "EasyTcpClient.hpp"
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
//��������
const int cCount = 8;
//�����߳�����
const int tCount = 4;
EasyTcpClient* client[cCount];

void sendThread(int id) {
	printf("thread = %d,start\n", id);
	int begin = cCount / tCount *(id - 1);
	int end = id * cCount / tCount;
	for (int i = begin; i < end; i++) {
		client[i] = new EasyTcpClient();
	}
	for (int i = begin; i < end; i++) {
		client[i]->Connect("127.0.0.1", 4567);
		//client[i]->Connect("116.57.115.67", 4567);//�����IP��ַ
	}
	printf("thread = %d, Connect<begin = %d, end = %d>\n", id, begin, end);
	std::chrono::milliseconds t(3000);
	std::this_thread::sleep_for(t);

	Login login[10];
	for (int i = 0; i < 10; i++) {
		strcpy(login[i].userName, "lyd");
		strcpy(login[i].PassWord, "lydmm");
	}
	const int nLen = sizeof(login);
	while (g_bRun) {
		for (int i = begin; i < end; i++) {
			client[i]->SendData(login, nLen);
			client[i]->OnRun();
		}
	}
	//�ر��׽���closesocket
	for (int i = begin; i < end; i++) {
		client[i]->Close();
		delete client[i];
	}
	printf("thread = %d,exit\n", id);
}

int main() {
	
	//�����߳�
	std::thread t1(cmdThread);
	t1.detach();

	//���������߳�
	for (int i = 0; i < tCount; i++) {
		std::thread t1(sendThread, i + 1);
		t1.detach();
	}
	while (g_bRun) {
		Sleep(100);
	}

	printf("�ͻ����Ѿ��˳����������...\n");
	return 0;

}
