
#include "reactor.hpp"
#include <iostream>
using namespace my_reactro;

int main() {
    reactor* rt = new reactor();
    
    rt->init();
    rt->run();

    return 0;
}