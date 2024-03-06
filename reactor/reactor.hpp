#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <functional>

#include <sys/sendfile.h>

#include <iostream>

#define BUFFER_LENGTH 1024
#define MAX_EVENTS_SIZE 1024
#define PORT 8888
#define HTMLPATH "./root/index.html"

namespace my_reactro {
    class reactor;

    struct conn_item {
        int fd;
        char rbuffer[BUFFER_LENGTH];
        int rlen;
        char wbuffer[BUFFER_LENGTH];
        int wlen;
    };
    class reactor {
    public:
        reactor() = default;
        int init() {
            this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serveraddr;
            
            memset(&serveraddr, 0, sizeof(struct sockaddr_in));

            serveraddr.sin_family = AF_INET;
            serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            serveraddr.sin_port = htons(PORT);

            int opt = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt)); // 防止偶发性的服务器无法进行立即重启(tcp协议的时候再说)


            if (bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr)) < 0) {
                perror("bind error");
                return -1;
            }

            
            listen(sockfd, 10);

            this->epfd = epoll_create(1);
            set_event(sockfd, EPOLLIN, EPOLL_CTL_ADD);
            connlist[sockfd].fd = sockfd;

            return 0;
        }
        int run() {
            while (1) {
                int nready = epoll_wait(this->epfd, events, MAX_EVENTS_SIZE, -1);  //阻塞等待
                for (int i = 0; i < nready; i++) {
                    int connfd = events[i].data.fd;
                    if (connfd == sockfd) {
                        int fd = accept_cb();
                    }
                    else if (events[i].events & EPOLLIN) {
                        int cnt = receive_cb(connfd);
                    } 
                    else if (events[i].events & EPOLLOUT) {
                        int cnt  = send_cb(connfd);
                    }
                }
            }
        }
    private:
        int accept_cb() {
            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);
            int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
            if (clientfd < 0) {
                return -1;
            }

            set_event(clientfd, EPOLLIN, EPOLL_CTL_ADD);
            connlist[clientfd].fd = clientfd;
            memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
            connlist[clientfd].rlen = 0;
            memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
            connlist[clientfd].wlen = 0;

            return clientfd;
        }
        int receive_cb(int fd) {
            char* buffer = connlist[fd].rbuffer;
            int idx = connlist[fd].rlen;    

            int cnt = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);
            std::cout << buffer << std::endl;        //打印接收到的数据

            if (cnt == 0) {
                //两个函数都需要做
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                return -1;
            }
            connlist[fd].rlen += cnt;

            set_event(fd, EPOLLOUT, EPOLL_CTL_MOD);
            return cnt;
        }
        int send_cb(int fd) {
#if 0
            /*  求文件内容长度
            std::ifstream in(htmlpath, std::ios::binary);
            if(!in.is_open()) return "";

            in.seekg(0, std::ios_base::end);
            auto len = in.tellg();
            in.seekg(0, std::ios_base::beg);
            */
#else 
        //wrk测试时段错误  : 原因大概出在open函数和stat上
        //    int filefd = open(HTMLPATH, O_RDONLY);

        //    off_t offset = 0;   //当前读到的偏移量
        //    struct stat stat_buf;
        //    fstat(filefd, &stat_buf);
        //    off_t size = stat_buf.st_size;

        //     while(offset < size) {
        //         ssize_t send_bytes = sendfile(fd, filefd, &offset, BUFFER_LENGTH);
        //         if (send_bytes < 0) {
        //             perror("sendfile failed");
        //             return 1;
        //         }
        //     }
        //   close(filefd);
#endif
            connlist[fd].wlen = sprintf(connlist[fd].wbuffer, 
            "HTTP/1.1 200 OK\r\n"
            "Accept-Ranges: bytes\r\n"
            "Content-Length: 82\r\n"
            "Content-Type: text/html\r\n"
            "Date: Sat, 06 Aug 2023 13:16:46 GMT\r\n\r\n"
            "<html><head><title>test</title></head><body><h1>test</h1></body></html>\r\n\r\n");
            int send_len = send(fd, connlist[fd].wbuffer, connlist[fd].wlen, 0);
            set_event(fd, EPOLLIN, EPOLL_CTL_MOD);
            return send_len;

        }

        void set_event(int fd, EPOLL_EVENTS event, int op) { 
            struct epoll_event ev;
            ev.events = event;      //EPOLL_EVENTS EPOLLIN/EPOLLOUT
            ev.data.fd = fd;
            epoll_ctl(this->epfd, op, fd, &ev);   //op: EPOLL_CTL_ADD : 1 , EPOLL_CTL_MOD:3  (epoll.h)
        }


    private:
        int sockfd;
        int epfd;
        struct epoll_event events[MAX_EVENTS_SIZE] = {0};
        conn_item connlist[MAX_EVENTS_SIZE];
    };
}
