#include <stdio.h> 
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>


#define PORT 8888

#define MAX_CONNECT 1024
#define ENTRIES_LENGTH		1024
#define BUFFER_LENGTH		1024

#define EVENT_ACCEPT   	0
#define EVENT_READ		1
#define EVENT_WRITE		2
#
struct conn_info {
	int fd;
	int event;
};

namespace ns_io_uring {
    class proactor {
    private:
        struct conn_item {
            conn_item() = default;
            ~conn_item() = default;
            int fd;
            char rbuffer[BUFFER_LENGTH];
            int rlen;
            char wbuffer[BUFFER_LENGTH];
            int wlen;
        };
    public:
        static proactor& get_instance() {   
            static proactor singleton;
            return singleton;
        }
        proactor& operator=(const proactor&) = delete;
        proactor(const proactor&) = delete;
        proactor() = default;

        int init() {
           init_socket();
           struct io_uring_params params;		
            memset(&params, 0, sizeof(params));
        
            io_uring_queue_init_params(ENTRIES_LENGTH, &this->ring, &params);  

            struct sockaddr_in clientaddr;	
            socklen_t len = sizeof(clientaddr);
            set_event_accept((struct sockaddr*)&clientaddr, &len, 0);
            return 0;
        }
        void run() {
            while (1) {
                io_uring_submit(&this->ring);		//封装：io_uring_enter, 将所有事件提交到内核

                struct io_uring_cqe *cqe;
                io_uring_wait_cqe(&this->ring, &cqe);		//拿到一个环形队列的头指针->cqe，阻塞

                struct io_uring_cqe *cqes[128];
                int nready = io_uring_peek_batch_cqe(&this->ring, cqes, 128);  // epoll_wait : 拿节点

                int i = 0;
                for (i = 0;i < nready;i ++) {
                    struct io_uring_cqe *entries = cqes[i];   //complete queue 的数据
                    struct conn_info result;
                    memcpy(&result, &entries->user_data, sizeof(struct conn_info));

                    if (result.event == EVENT_ACCEPT) {
                        struct sockaddr_in clientaddr;	
                        socklen_t len = sizeof(clientaddr);
                        set_event_accept((struct sockaddr*)&clientaddr, &len, 0);		//拿取之后需要再set
                        //printf("set_event_accept\n"); //
                        //异步调用，返回到struct io_uring_cqe *entries = cqes[i]中，此时accept返回的是clientfd
                        int connfd = entries->res;	
                        init_connect_item(connfd);

                        set_event_recv(connfd, connect_list[connfd].rbuffer, BUFFER_LENGTH, 0);
                    } else if (result.event == EVENT_READ) {  //与EPOLLIN的区别：当EPOLLIN时表示可读，而当这里的事件发生时已经读完了
                        //异步，所以这里ret是已经读取的数据长度
                        int ret = entries->res;
                        //printf("set_event_recv ret: %d, %s\n", ret, buffer); //
                        if (ret == 0) {
                            close(result.fd);
                        } else if (ret > 0) {
                            //"kvstore.hpp"
                            int clientfd = result.fd;
                            char* rbuffer = this->connect_list[clientfd].rbuffer;
                            rbuffer[ret] = 0;    //!!!!防止读到以前的数据

                            set_event_send(result.fd, connect_list[clientfd].rbuffer, ret, 0);
                        }
                    }  else if (result.event == EVENT_WRITE) {
                        int ret = entries->res;
                        // printf("set_event_send ret: %d, %s\n", ret, buffer);

                        set_event_recv(result.fd, connect_list[result.fd].rbuffer, BUFFER_LENGTH, 0);
                        
                    }
                    
                }

                io_uring_cq_advance(&this->ring, nready);	//清空complete queue
            }
        }

    private:
        void set_event_recv(int clientfd, void *buf, size_t len, int flags) {

            struct io_uring_sqe *sqe = io_uring_get_sqe(&this->ring);    //获取队列头

            struct conn_info accept_info = {
                .fd = clientfd,
                .event = EVENT_READ,
            };
            io_uring_prep_recv(sqe, clientfd, buf, len, flags);
            memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));

        }

        void set_event_send(int clientfd, void *buf, size_t len, int flags) {

            struct io_uring_sqe *sqe = io_uring_get_sqe(&this->ring);

            struct conn_info accept_info = {
                .fd = clientfd,
                .event = EVENT_WRITE,
            };
            
            io_uring_prep_send(sqe, clientfd, buf, len, flags);
            memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));

        }

        void set_event_accept(struct sockaddr *addr, socklen_t *addrlen, int flags) {

            struct io_uring_sqe *sqe = io_uring_get_sqe(&this->ring);  //拿到接收队列

            struct conn_info accept_info = {    //像epoll一样初始化事件
                .fd = this->listenfd,
                .event = EVENT_ACCEPT,
            };
            
            //提交请求到sqe里 ,对应accept :封装 io_uring_register，异步
            io_uring_prep_accept(sqe, this->listenfd, (struct sockaddr*)addr, addrlen, flags);  		//这里不会阻塞，异步accept
            memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));

        }

        void init_connect_item(int clientfd) {
            this->connect_list[clientfd].fd = clientfd;
            memset(this->connect_list[clientfd].rbuffer, 0, BUFFER_LENGTH);
            this->connect_list[clientfd].rlen = 0;
            memset(this->connect_list[clientfd].wbuffer, 0, BUFFER_LENGTH);
            this->connect_list[clientfd].wlen = 0;
        }

        void init_socket() {
            this->listenfd = socket(AF_INET, SOCK_STREAM, 0);	
            struct sockaddr_in serveraddr;	
            memset(&serveraddr, 0, sizeof(struct sockaddr_in));	
            serveraddr.sin_family = AF_INET;	
            serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);	
            serveraddr.sin_port = htons(PORT);	

            int opt = 1;
            setsockopt(this->listenfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt)); // 防止偶发性的服务器无法进行立即重启(tcp协议的时候再说)

            if (-1 == bind(this->listenfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {		
                perror("bind");	
                return;	
            }	
            listen(this->listenfd, 10);
            
        }
    private:
        int listenfd;
        struct io_uring ring;
        struct conn_item connect_list[MAX_CONNECT];
    };
}