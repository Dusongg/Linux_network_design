cmake_minimum_required(VERSION 3.5.1)

project(reactor)

# include_directories(include)
set(CMAKE_CXX_STANDARD 11)

add_executable(reactor reactor.cc)

target_link_libraries(reactor pthread)