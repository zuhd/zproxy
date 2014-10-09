#include "socket_server.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>


struct sock5req1 
{ 
	char Ver; 
	char nMethods; 
	char Methods[255]; 
}; 

struct sock5ans1 
{ 
	char Ver; 
	char Method; 
}; 

struct sock5req2 
{ 
	char Ver; 
	char Cmd; 
	char Rsv; 
	char Atyp; 
	unsigned long IPAddr;
	unsigned short Port;
}; 

struct sock5ans2 
{ 
	char Ver; 
	char Rep; 
	char Rsv; 
	char Atyp; 
	char other[1]; 
}; 

struct authreq 
{ 
	char Ver; 
	char Ulen; 
	char Name[255]; 
	char PLen; 
	char Pass[255]; 
}; 

struct authans 
{ 
	char Ver; 
	char Status; 
}; 


//opaque 相当于Session的指针地址

struct st_pair
{
	int pair_opaque;
	int pair_recvid;
	int pair_sendid;	
};

#define MAX_PAIR 1024
int nOpaque = 200;
int nPairIndex = 0;
struct st_pair pair[MAX_PAIR];

static void 
on_connection(struct socket_server *ss, uintptr_t opaque, int id)
{
	// 换转一下pair
	int i = 0;
	for (i = 0; i < MAX_PAIR; i++)
	{
		if (pair[i].pair_opaque == opaque)
		{
			printf("opaque=%d, recv id = %d, send id = %d \n", opaque, pair[i].pair_recvid, id);
			pair[i].pair_sendid = id;
			//反向映射一次
			pair[nPairIndex].pair_opaque = opaque;
			pair[nPairIndex].pair_recvid = id;
			pair[nPairIndex].pair_sendid = pair[i].pair_recvid;
			nPairIndex++;
			break;
		}
	}
}

static void 
on_recv(struct socket_server *ss, int id, const char* pData, int nLen)
{
	// 针对SOCKS5代理协议
	struct sock5req1* req1 = (struct sock5req1*)pData;
	struct sock5req2* req2 = (struct sock5req2*)pData;
	const char* p1 = strstr(pData, "CONNECT");
	const char* p2 = strstr(pData, "HTTP");
	const char* p3 = strstr(pData, ":");
	
	
	if (req2 != NULL &&
		nLen >= 10)
	{
		if (req2->Ver == 5 &&
			req2->Cmd == 1 &&
			req2->Rsv == 0 &&
			req2->Atyp == 1)
		{
			struct sock5ans2 ans2;
			ans2.Ver = 5;
			ans2.Rep = 0;
			char* pRet = (char*)malloc(128);
			memset(pRet, 0, 128);
			memcpy(pRet, &ans2, sizeof(ans2));
			socket_server_send(ss, id,pRet, sizeof(ans2));
			// 连接目标服务器
			struct in_addr inaddr;
			inaddr.s_addr = req2->IPAddr;			
			socket_server_connect(ss,nOpaque,inet_ntoa(inaddr),ntohs(req2->Port));
			pair[nPairIndex].pair_opaque = nOpaque;
			pair[nPairIndex].pair_recvid = id;
			nPairIndex++;
			nOpaque++;
			printf("SOCKS5 addr=%s, port=%d\n", inet_ntoa(inaddr), ntohs(req2->Port));
			return;
		}
	}	
	if (req1 != NULL &&
		nLen >= 2)
	{
		if (req1->Ver == 5 &&
			req1->nMethods == 1)
		{
			struct sock5ans1 ans1;
			ans1.Ver = 5;
			ans1.Method = 0;
			char* pRet = (char*)malloc(128);	
			memset(pRet, 0, 128);
		 	memcpy(pRet, &ans1, sizeof(ans1));
			socket_server_send(ss, id, pRet, sizeof(ans1));
			return;
		}
	}	
	
	if (p1 != NULL &&
		p2 != NULL)
	{
		// HTTP代理协议
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
		socket_server_connect(ss,nOpaque,szIP,nPort);
		pair[nPairIndex].pair_opaque = nOpaque;
		pair[nPairIndex].pair_recvid = id;
		nPairIndex++;
		nOpaque++;
		return;
	}	
	// 直接转发消息
	int i = 0;
	for (i = 0; i < MAX_PAIR; i++)
	{
		if (pair[i].pair_recvid == id)
		{
			if (pair[i].pair_sendid > 0)
			{				
				char* pCopy = (char*)malloc(nLen);
				memcpy(pCopy, pData, nLen);
				printf("recvid=%d, sendid=%d, data=%s, len=%d\n", pair[i].pair_recvid, pair[i].pair_sendid, pCopy, nLen);
				socket_server_send(ss, pair[i].pair_sendid, pCopy, nLen);
				break;
			}
		}
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
			on_recv(ss, result.id, result.data, result.ud);
			//TODO with the opaque
			free(result.data);			
			break;
		case SOCKET_CLOSE:
			printf("close(%lu) [id=%d]\n",result.opaque,result.id);
			break;
		case SOCKET_OPEN:
			printf("open(%lu) [id=%d] %s\n",result.opaque,result.id,result.data);
			on_connection(ss, result.opaque, result.id);
			break;
		case SOCKET_ERROR:
			printf("error(%lu) [id=%d]\n",result.opaque,result.id);
			break;
		case SOCKET_ACCEPT:
			printf("accept(%lu) [id=%d %s] from [%d]\n",result.opaque, result.ud, result.data, result.id);
			//在这儿要关联一个新的opaque
			socket_server_start(ss,nOpaque++,result.ud);
			break;
		}
	}
}

static void
begin_listen(struct socket_server *ss) {
	pthread_t pid;
	pthread_create(&pid, NULL, _poll, ss);
	
	int l = socket_server_listen(ss,nOpaque,"192.168.17.101",8888,32);
	printf("listening %d\n",l);
	socket_server_start(ss,nOpaque++,l);	
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
	begin_listen(ss);
	socket_server_release(ss);

	return 0;
}
