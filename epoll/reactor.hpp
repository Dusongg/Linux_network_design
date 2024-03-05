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

#define BUFFER_LENGTH 1024
#define MAX_EVENTS_SIZE 1024
namespace my_reactro {
    struct conn_item {
        int fd;
        char rbuffer[BUFFER_LENGTH];
        int rlen;
        char wbuffer[BUFFER_LENGTH];
        int wlen;

        union {
            std::function<int(reactor* ts, int)> accept_func;
            std::function<int(reactor* ts, int)> receive_func;
        } arfunc_t;
        std::function<int(reactor* ts, int)> send_func;
    };
    class reactor {
    public:
        reactor() = default;
        ~reactor() = default;

        int init() {
            this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serveraddr;
            
            memset(&serveraddr, 0, sizeof(struct sockaddr_in));

            serveraddr.sin_family = AF_INET;
            serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            serveraddr.sin_port = htons(8888);

            if (bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr)) < 0) {
                perror("bind error");
                return -1;
            }
            listen(sockfd, 10);

            this->epfd = epoll_create(1);
            set_event(sockfd, EPOLLIN, EPOLL_CTL_ADD);
            connlist[sockfd].fd = sockfd;
            connlist[sockfd].arfunc_t.accept_func = accept_cb;

            return 0;
        }
        int run() {
            while (1) {
                int nready = epoll_wait(this->epfd, events, MAX_EVENTS_SIZE, -1);  //阻塞等待
                for (int i = 0; i < nready; i++) {
                    int connfd = events[i].data.fd;
                    if (events[i].events & EPOLLIN) {
                        int cnt = connlist[connfd].arfunc_t.receive_func(this, connfd);
                    } 
                    else if (events[i].events & EPOLLOUT) {
                        int cnt  = connlist[connfd].send_func(this, connfd);
                    }
                }
            }
        }
    private:
        int accept_cb(int) {
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

            connlist[clientfd].arfunc_t.receive_func = receive_cb;
            connlist[clientfd].send_func = send_cb;
        }
        int receive_cb(int fd) {
            char* buffer = connlist[fd].rbuffer;
            int idx = connlist[fd].rlen;    

            int cnt = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);
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
            int filefd = open("./root/index.html", O_RDONLY);
            /*  求文件内容长度
            std::ifstream in(htmlpath, std::ios::binary);
            if(!in.is_open()) return "";

            in.seekg(0, std::ios_base::end);
            auto len = in.tellg();
            in.seekg(0, std::ios_base::beg);
            */
           struct stat stat_buf;
           fstat(filefd, &stat_buf);
           int size = stat_buf.st_size;

           sendfile(fd, filefd, NULL, size);

           set_event(fd, EPOLLIN, EPOLL_CTL_MOD);
           return size;

        }

        void set_event(int fd, EPOLL_EVENTS event, int op) { 
            struct epoll_event ev;
            ev.events = event;      //EPOLL_EVENTS:EPOLLIN / EPOLLOUT
            ev.data.fd = fd;
            epoll_ctl(this->epfd, op, fd, &ev);   //op: EPOLL_CTL_ADD : 1 , EPOLL_CTL_MOD:3  (epoll.h)
        }


    private:
        int sockfd;
        int epfd;
        struct epoll_event events[MAX_EVENTS_SIZE];
        conn_item connlist[MAX_EVENTS_SIZE];
    };
}
