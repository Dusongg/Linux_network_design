#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define PORT 8888


namespace ns_io_uring {
    class io_uring {
    public:
        static io_uring& get_instance() {   
            static io_uring singleton;
            return singleton;
        }
        io_uring& operator=(const io_uring&) = delete;
        io_uring(const io_uring&) = delete;

        int init() {
           


        }
        void run() {

        }



    private:
    void init_socket() {
        this->sockfd = socket(AF_INET, SOCK_STREAM, 0);	
        struct sockaddr_in serveraddr;	
        memset(&serveraddr, 0, sizeof(struct sockaddr_in));	
        serveraddr.sin_family = AF_INET;	
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);	
        serveraddr.sin_port = htons(PORT);	

        if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {		
            perror("bind");	
            return;	
        }	
        listen(sockfd, 10);
    }
    private:
        int sockfd;
        io_uring() = default;
    };
}