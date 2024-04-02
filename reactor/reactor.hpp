// #include <sys/socket.h>
#include "../socket/socket.hpp"
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

#include <mutex>


#define BUFFER_LENGTH 1024
#define MAX_EVENTS_SIZE 1024
#define HTMLPATH "./root/index.html"

namespace my_reactro {
    class reactor;
    struct conn_item {
        conn_item() {};
        ~conn_item() {};
        union ar_func_t {
            ar_func_t() {};
            ~ar_func_t() {};
            std::function<int(reactor* ts, int)> accept_func;
            std::function<int(reactor* ts, int)> receive_func;
        };
        int fd;
        char rbuffer[BUFFER_LENGTH];
        int rlen;
        char wbuffer[BUFFER_LENGTH];
        int wlen;

        ar_func_t ar_func;
        std::function<int(reactor* ts, int)> send_func;
    };  


    class reactor {
    public:
        static reactor* get_Singleton() {
            if (singleton == nullptr) {   //
                std::lock_guard<std::mutex> lock(init_mutex);
                if (singleton == nullptr) {
                    singleton = new reactor();
                }
            } 
            return singleton;
        }
        ~reactor() {
            close(epfd);
        }
        reactor(const reactor&) = delete;
        reactor& opeator(const reactor&) = delete;

    public:
        int init() {
            int listenfd = _socket.get_sockfd();
            //端口复用
            int opt = 1;
            setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt));
            
            _socket.Bind(8888);
            _socket.Listen();

            this->epfd = epoll_create(1);
            set_event(listenfd, EPOLL_CTL_ADD, EPOLLIN);
            connlist[listenfd].fd = listenfd;
            connlist[listenfd].ar_func.accept_func = accept_cb;

            return 0;
        }
        int run() {
            while (1) {
                int nready = epoll_wait(this->epfd, events, MAX_EVENTS_SIZE, -1);  //阻塞等待
                for (int i = 0; i < nready; i++) {
                    int connfd = events[i].data.fd;
                    if (events[i].events & EPOLLIN) {
                        int cnt = connlist[connfd].ar_func.receive_func(this, connfd);
                    } 
                    else if (events[i].events & EPOLLOUT) {
                        int cnt  = connlist[connfd].send_func(this, connfd);
                    }
                }
            }
        }
    private:

        static int accept_cb(reactor* ts, int) {
            int clientfd = ts->_socket.Accept();

            ts->set_event(clientfd, EPOLL_CTL_ADD, EPOLLIN);
            ts->connlist[clientfd].fd = clientfd;
            memset(ts->connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
            ts->connlist[clientfd].rlen = 0;
            memset(ts->connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
            ts->connlist[clientfd].wlen = 0;

            ts->connlist[clientfd].ar_func.receive_func = receive_cb;
            ts->connlist[clientfd].send_func = send_cb;

            return clientfd;
        }
        static int receive_cb(reactor* ts, int fd) {
            char* buffer = ts->connlist[fd].rbuffer;
            int idx = ts->connlist[fd].rlen;    

            int cnt = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);
            // std::cout << buffer << std::endl;        //打印接收到的数据

            if (cnt == 0) {
                //两个函数都需要做
                epoll_ctl(ts->epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                return -1;
            }
            ts->connlist[fd].rlen += cnt;

            ts->set_event(fd, EPOLL_CTL_MOD, EPOLLOUT);
            return cnt;
        }
        static int send_cb(reactor* ts, int fd) {
            int ret = sprintf(ts->connlist[fd].rbuffer, 
                "HTTP/1.1 200 OK\r\n"
                "Accept-Ranges: bytes\r\n"
                "Content-Length: 75\r\n"
                "Content-Type: text/html\r\n"
                "Date: Sat, 06 Aug 2023 13:16:46 GMT\r\n\r\n"
                "<html><head><title>Dusong</title></head><body><h1>Dusong</h1></body></html>\r\n");
            int send_len = send(fd, ts->connlist[fd].rbuffer, ts->connlist[fd].rlen, 0);
            ts->set_event(fd,  EPOLL_CTL_MOD, EPOLLIN);
            return send_len;
        }

        void set_event(int fd, int op, EPOLL_EVENTS event) { 
            struct epoll_event ev;
            ev.events = event;      //EPOLL_EVENTS EPOLLIN/EPOLLOUT
            ev.data.fd = fd;
            epoll_ctl(this->epfd, op, fd, &ev);   //op: EPOLL_CTL_ADD : 1 , EPOLL_CTL_MOD:3  (epoll.h)
        }


    private:
        reactor() {}
        static reactor* singleton;
        Socket _socket;
        int epfd;
        struct epoll_event events[MAX_EVENTS_SIZE];
        conn_item connlist[MAX_EVENTS_SIZE];
        static std::mutex init_mutex;
    };
    std::mutex reactor::init_mutex;
    reactor* reactor::singleton = nullptr;
}
