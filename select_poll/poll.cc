#include <poll.h>
#include "../socket/socket.hpp"
int main() {
    Socket _socket;
    int sockfd = _socket.get_sockfd();
    _socket.Bind(8888);
    _socket.Listen(); 

    struct pollfd fds[1024];
    fds[sockfd].fd = sockfd;
    fds[sockfd].events = POLLIN;

    int mxfd = sockfd;

    while (true){
        //timeout == -1 阻塞等待    
        int nready = poll(fds, mxfd + 1, -1);

        if (fds[sockfd].revents & POLLIN) {
            int clientfd = _socket.Accept();
            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;
            mxfd = std::max(mxfd, clientfd);     //考虑之前的文件描述符关闭情况
            DBG_LOG("%d", clientfd);
        }

        for (int fd = sockfd + 1; fd < mxfd + 1; fd++) {
            //这里只处理读事件
            if (fds[fd].revents & POLLIN) {
                char buffer[128] = {0};
				int count = recv(fd, buffer, 128, 0);
				if (count == 0) {
					fds[fd].fd = -1;
                    fds[fd].events = 0;
                    close(fd);
                    continue;    
				}
				send(fd, buffer, count, 0);
            }

        }
        
    }
    


}