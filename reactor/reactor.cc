
#include "reactor.hpp"
#include <iostream>
#include <memory>
using namespace my_reactro;

int main() {
    // reactor* rt = new reactor();
    
    // rt->init();
    // rt->run();
    std::unique_ptr<reactor> singleton_reactor(reactor::get_Singleton());
    singleton_reactor->init();
    singleton_reactor->run();
    return 0;
}