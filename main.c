#include "socket_server.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


//opaque 相当于Session的指针地址
static void 
OnRecv(struct socket_server *ss, int id, const char* pData, int nLen)
{
	const char* p1 = strstr(pData, "CONNECT");
	const char* p2 = strstr(pData, "HTTP");
	const char* p3 = strstr(pData, ":");
	if (p1 != NULL &&
		p2 != NULL)
	{
		char szIP[32] = {0};
		strncpy(szIP, p1+8, p3-p1-8);
		printf("%s\n", szIP);
		char szPort[16] = {0};
		strncpy(szPort, p3+1, p2-p3-1);
		int nPort = atoi(szPort);
		printf("%d\n", nPort);
		char* pRet = (char*)malloc(128);		
		strncpy(pRet, "HTTP/1.0 200 Connection established", 128);
		socket_server_send(ss, id, pRet, strlen(pRet));
		// 连接目标服务器
	}
	else
	{
		// 直接转发消息
	}
}

static void *
_poll(void * ud) {	
	struct socket_server *ss = ud;
	struct socket_message result;
	for (;;) {
		int type = socket_server_poll(ss, &result, NULL);
		// DO NOT use any ctrl command (socket_server_close , etc. ) in this thread.
		switch (type) {
		case SOCKET_EXIT:
			return NULL;
		case SOCKET_DATA:
			printf("message(%lu) [id=%d] size=%d data=%s\n",result.opaque,result.id, result.ud, result.data);
			OnRecv(ss, result.id, result.data, result.ud);
			//TODO with the opaque
			free(result.data);			
			break;
		case SOCKET_CLOSE:
			printf("close(%lu) [id=%d]\n",result.opaque,result.id);
			break;
		case SOCKET_OPEN:
			printf("open(%lu) [id=%d] %s\n",result.opaque,result.id,result.data);
			break;
		case SOCKET_ERROR:
			printf("error(%lu) [id=%d]\n",result.opaque,result.id);
			break;
		case SOCKET_ACCEPT:
			printf("accept(%lu) [id=%d %s] from [%d]\n",result.opaque, result.ud, result.data, result.id);
			//在这儿要关联一个新的opaque
			socket_server_start(ss,201,result.ud);
			break;
		}
	}
}

static void
test(struct socket_server *ss) {
	pthread_t pid;
	pthread_create(&pid, NULL, _poll, ss);
	
	int l = socket_server_listen(ss,200,"192.168.17.101",8888,32);
	printf("listening %d\n",l);
	socket_server_start(ss,200,l);	
	sleep(5);
	//socket_server_exit(ss);

	pthread_join(pid, NULL); 
}

int
main() {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, 0);

	struct socket_server * ss = socket_server_create();
	test(ss);
	socket_server_release(ss);

	return 0;
}
