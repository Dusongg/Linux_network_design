#include <sys/select.h>
#include "../socket/socket.hpp"
int main() {
    Socket _socket;
    int sockfd = _socket.get_sockfd();
    _socket.Bind(8888);
    _socket.Listen(); 

    fd_set rset, rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    int mxfd = sockfd;
    while (true){
        rset = rfds;
        //timeout == nullptr：阻塞直到有文件描述符就绪
        int nready = select(mxfd + 1, &rset, nullptr, nullptr, nullptr);
        
        if (FD_ISSET(sockfd, &rset)) {
            int clientfd = _socket.Accept();
            FD_SET(clientfd, &rfds);
            mxfd = std::max(mxfd, clientfd);     //考虑之前的文件描述符关闭情况

        }

        for (int fd = sockfd + 1; fd < mxfd + 1; fd++) {
            //这里只处理读事件
            if (FD_ISSET(fd, &rset)) {
                char buffer[128] = {0};
				int count = recv(fd, buffer, 128, 0);
				if (count == 0) {
					FD_CLR(fd, &rfds);
					close(fd);
					continue;
				}
				send(fd, buffer, count, 0);
            }
        }
        
    }
    


}