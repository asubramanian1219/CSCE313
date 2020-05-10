cmake_minimum_required(VERSION 3.16)
project(PA5_Bonus)

set(CMAKE_CXX_STANDARD 14)

add_executable(PA5_Bonus client.cpp server.cpp MQreqchannel.cpp Histogram.cpp FIFOreqchannel.cpp common.cpp )