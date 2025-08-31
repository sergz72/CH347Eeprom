cmake_minimum_required(VERSION 4.0)
project(CH347Eeprom C)

set(CMAKE_C_STANDARD 23)

add_executable(CH347Eeprom main.c)
target_link_libraries(CH347Eeprom libch347.so)
