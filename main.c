#include "socket_server.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static void 
OnRecv(const char* pData, int nLen)
{
}

static void *
_poll(void * ud) {
	char* szbuff = malloc(1024);
	strncpy(szbuff, "abcefg", 6);
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
			OnRecv(result.data, result.ud);
			//TODO with the opaque
			free(result.data);
			//socket_server_send(ss,result.id,szbuff, strlen(szbuff));
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
			socket_server_start(ss,201,result.ud);
			break;
		}
	}
}

static void
test(struct socket_server *ss) {
	pthread_t pid;
	pthread_create(&pid, NULL, _poll, ss);

	//int c = socket_server_connect(ss,100,"127.0.0.1",80);
	//printf("connecting %d\n",c);
	int l = socket_server_listen(ss,200,"192.168.17.101",8888,32);
	printf("listening %d\n",l);
	socket_server_start(ss,201,l);
	// some misunderstanding about bind
	//int b = socket_server_bind(ss,300,1);
	//printf("binding stdin %d\n",b);
	//int i;
	//for (i=0;i<100;i++) {
	//	socket_server_connect(ss, 400+i, "127.0.0.1", 8888);
	//}
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
