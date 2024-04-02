#include "reactor.hpp"

int main() {
    my_reactro::reactor* server = my_reactro::reactor::get_Singleton();
    server->init();
    server->run();
}