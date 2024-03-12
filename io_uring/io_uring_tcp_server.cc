#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>


#define EVENT_ACCEPT   	0
#define EVENT_READ		1
#define EVENT_WRITE		2

struct conn_info {
	int fd;
	int event;
};


int init_server(unsigned short port) {	

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);	
	struct sockaddr_in serveraddr;	
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));	
	serveraddr.sin_family = AF_INET;	
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);	
	serveraddr.sin_port = htons(port);	

	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {		
		perror("bind");		
		return -1;	
	}	

	listen(sockfd, 10);
	
	return sockfd;
}



#define ENTRIES_LENGTH		1024
#define BUFFER_LENGTH		1024

int set_event_recv(struct io_uring *ring, int sockfd,
				      void *buf, size_t len, int flags) {

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);    //获取队列头

	struct conn_info accept_info = {
		.fd = sockfd,
		.event = EVENT_READ,
	};
	
	io_uring_prep_recv(sqe, sockfd, buf, len, flags);
	memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));

}


int set_event_send(struct io_uring *ring, int sockfd,
				      void *buf, size_t len, int flags) {

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

	struct conn_info accept_info = {
		.fd = sockfd,
		.event = EVENT_WRITE,
	};
	
	io_uring_prep_send(sqe, sockfd, buf, len, flags);
	memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));

}



int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr,
					socklen_t *addrlen, int flags) {

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);  //拿到接收队列

	struct conn_info accept_info = {    //像epoll一样初始化事件
		.fd = sockfd,
		.event = EVENT_ACCEPT,
	};
	
	//提交请求到sqe里 ,对应accept :封装 io_uring_register，异步
	io_uring_prep_accept(sqe, sockfd, (struct sockaddr*)addr, addrlen, flags);  		//这里不会阻塞，异步accept
	memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));

}




int main(int argc, char *argv[]) {

	unsigned short port = 8888;
	int sockfd = init_server(port);

	struct io_uring_params params;		//？
	memset(&params, 0, sizeof(params));

	struct io_uring ring;
	io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);    //构建两个队列：封装io_uring_setup

	
#if 0
	struct sockaddr_in clientaddr;	
	socklen_t len = sizeof(clientaddr);
	accept(sockfd, (struct sockaddr*)&clientaddr, &len);
#else

	struct sockaddr_in clientaddr;	
	socklen_t len = sizeof(clientaddr);
	set_event_accept(&ring, sockfd, (struct sockaddr*)&clientaddr, &len, 0);
	
#endif

	char buffer[BUFFER_LENGTH] = {0};

	while (1) {

		io_uring_submit(&ring);		//封装：io_uring_enter, 将所有事件提交到内核


		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring, &cqe);		//拿到一个环形队列的头指针->cqe，阻塞

		struct io_uring_cqe *cqes[128];
		int nready = io_uring_peek_batch_cqe(&ring, cqes, 128);  // epoll_wait : 拿节点

		int i = 0;
		for (i = 0;i < nready;i ++) {

			struct io_uring_cqe *entries = cqes[i];   //complete queue 的数据
			struct conn_info result;
			memcpy(&result, &entries->user_data, sizeof(struct conn_info));

			if (result.event == EVENT_ACCEPT) {

				set_event_accept(&ring, sockfd, (struct sockaddr*)&clientaddr, &len, 0);		//拿取之后需要再set
				//printf("set_event_accept\n"); //
				
				//异步调用，返回到struct io_uring_cqe *entries = cqes[i]中，此时accept返回的是clientfd
				int connfd = entries->res;		

				set_event_recv(&ring, connfd, buffer, BUFFER_LENGTH, 0);

				
			} else if (result.event == EVENT_READ) {  //与EPOLLIN的区别：当EPOLLIN时表示可读，而当这里的事件发生时已经读完了
				//异步，所以这里ret是已经读取的数据长度
				int ret = entries->res;
				//printf("set_event_recv ret: %d, %s\n", ret, buffer); //

				if (ret == 0) {
					close(result.fd);
				} else if (ret > 0) {
					set_event_send(&ring, result.fd, buffer, ret, 0);
				}
			}  else if (result.event == EVENT_WRITE) {
  //

				int ret = entries->res;
				//printf("set_event_send ret: %d, %s\n", ret, buffer);

				set_event_recv(&ring, result.fd, buffer, BUFFER_LENGTH, 0);
				
			}
			
		}

		io_uring_cq_advance(&ring, nready);	//清空complete queue
	}

}


