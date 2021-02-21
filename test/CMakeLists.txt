# cmake_minimum_required(VERSION <specify CMake version here>)
project(Test C)

set(CMAKE_C_STANDARD 99)

#add_executable(Test src/main.c)
add_executable(Test src/udpc.c)

add_executable(udpc src/udpc.c)
add_executable(udps src/udps.c)
add_executable(tcp2c src/tcp2c.c)
add_executable(tcps src/tcps.c)

# To compile and run in terminal:
#### gcc src/udpc.c -o build/udpc && build/udpc
#### gcc src/udps.c -o build/udps && build/udps