

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <unistd.h>

#include <pthread.h>
#include <sys/epoll.h>
#include <string.h>


#define BUFFER_LENGTH	128
#define EVENTS_LENGTH	128


char rbuffer[BUFFER_LENGTH] = {0};
char wbuffer[BUFFER_LENGTH] = {0};

// listenfd, clientfd
struct sock_item { // conn_item

	int fd; // clientfd

	char *rbuffer;
	int rlength; //

	char *wbuffer;
	int wlength;
	
	int event;

	void (*recv_cb)(int fd, char *buffer, int length);
	void (*send_cb)(int fd, char *buffer, int length);

	void (*accept_cb)(int fd, char *buffer, int length);

};


struct reactor {
	int epfd;
	
};

// thread --> fd
void *routine(void *arg) {

	int clientfd = *(int *)arg;

	while (1) {
		
		unsigned char buffer[BUFFER_LENGTH] = {0};
		int ret = recv(clientfd, buffer, BUFFER_LENGTH, 0);
		if (ret == 0) {
			close(clientfd);
			break;
			
		}
		printf("buffer : %s, ret: %d\n", buffer, ret);

		ret = send(clientfd, buffer, ret, 0); // 

	}

}

// socket --> 
// bash --> execve("./server", "");
// 
// 0, 1, 2
// stdin, stdout, stderr
int main() {

// block
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);  // 
	if (listenfd == -1) return -1;
// listenfd
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(9999);

	if (-1 == bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
		return -2;
	}

#if 0 // nonblock
	int flag = fcntl(listenfd, F_GETFL, 0);
	flag |= O_NONBLOCK;
	fcntl(listenfd, F_SETFL, flag);
#endif

	listen(listenfd, 10);

#if 0
	// int 
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	int clientfd = accept(listenfd, (struct sockaddr*)&client, &len);

	unsigned char buffer[BUFFER_LENGTH] = {0};
	int ret = recv(clientfd, buffer, BUFFER_LENGTH, 0);
	if (ret == 0) {
		close(clientfd);
		
	}
	printf("buffer : %s, ret: %d\n", buffer, ret);

	ret = send(clientfd, buffer, ret, 0); // 

	//printf("sendbuffer : %d\n", ret);
#elif 0

	while (1) {

		
		struct sockaddr_in client;
		socklen_t len = sizeof(client);
		int clientfd = accept(listenfd, (struct sockaddr*)&client, &len);
		
		pthread_t threadid;
		pthread_create(&threadid, NULL, routine, &clientfd);

		//fork();

	}
	

#elif 0

	fd_set rfds, wfds, rset, wset;

	FD_ZERO(&rfds);
	FD_SET(listenfd, &rfds);

	FD_ZERO(&wfds);

	int maxfd = listenfd;

	unsigned char buffer[BUFFER_LENGTH] = {0}; // 0 
	int ret = 0;

	// int fd, 
	while (1) {

		rset = rfds;
		wset = wfds;

		int nready = select(maxfd+1, &rset, &wset, NULL, NULL);
		if (FD_ISSET(listenfd, &rset)) {

			printf("listenfd --> \n");

			struct sockaddr_in client;
			socklen_t len = sizeof(client);
			int clientfd = accept(listenfd, (struct sockaddr*)&client, &len);
			
			FD_SET(clientfd, &rfds);

			if (clientfd > maxfd) maxfd = clientfd;

			
			
		} 
		
		int i = 0;
		for (i = listenfd+1; i <= maxfd;i ++) {

			if (FD_ISSET(i, &rset)) { //

				ret = recv(i, buffer, BUFFER_LENGTH, 0);
				if (ret == 0) {
					close(i);
					FD_CLR(i, &rfds);
					
				} else if (ret > 0) {
					printf("buffer : %s, ret: %d\n", buffer, ret);
					FD_SET(i, &wfds);
				}
				
			} else if (FD_ISSET(i, &wset)) {
				
				ret = send(i, buffer, ret, 0); // 
				
				FD_CLR(i, &wfds); //
				FD_SET(i, &rfds);
				

			}

		}
		
		// 

	}

#else

// fd --> epoll 
	int epfd = epoll_create(1);

	struct epoll_event ev, events[EVENTS_LENGTH];
	ev.events = EPOLLIN;
	ev.data.fd = listenfd; //

	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev); // 
	

	while (1) { // 7 * 24 

		int nready = epoll_wait(epfd, events, EVENTS_LENGTH, -1); // -1, ms 
		printf("------- %d\n", nready);
		
		int i = 0;
		for (i = 0;i < nready;i ++) {
			int clientfd= events[i].data.fd;
			
			if (listenfd == clientfd) { // accept

				//while(1) {
					struct sockaddr_in client;
					socklen_t len = sizeof(client);
					int connfd = accept(listenfd, (struct sockaddr*)&client, &len);
					if (connfd == -1) break;
					
					printf("accept: %d\n", connfd);
					ev.events = EPOLLIN;
					ev.data.fd = connfd;
					epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
				//}
			

			} else if (events[i].events & EPOLLIN) { //clientfd

				//char rbuffer[BUFFER_LENGTH] = {0};

				int n = recv(clientfd, rbuffer, BUFFER_LENGTH, 0);
				if (n > 0) {
					//rbuffer[n] = '\0';

					printf("recv: %s, n: %d\n", rbuffer, n);

					memcpy(wbuffer, rbuffer, BUFFER_LENGTH);

					ev.events = EPOLLOUT;
					ev.data.fd = clientfd;

					epoll_ctl(epfd, EPOLL_CTL_MOD, clientfd, &ev);
					
				} 
				
			} else if (events[i].events & EPOLLOUT) {

				
				int sent = send(clientfd, wbuffer, BUFFER_LENGTH, 0); //
				printf("sent: %d\n", sent);

				ev.events = EPOLLIN;
				ev.data.fd = clientfd;

				epoll_ctl(epfd, EPOLL_CTL_MOD, clientfd, &ev);
				
				
			}

		}

	}
	
	


#endif

}




