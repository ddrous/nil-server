# cmake_minimum_required(VERSION <specify CMake version here>)
project(Nil C)

set(CMAKE_C_STANDARD 99)

add_executable(main src/main.c)

add_executable(client src/client.c)
add_executable(librairie src/librairie.c)
add_executable(nil src/nil.c)


# TEST
## librarie: gcc src/librairie.c -g -o cmake-build-debug/librairie && cmake-build-debug/librairie 9001 Zoro A Dune Two
##          - gcc src/librairie.c -g -o cmake-build-debug/librairie && cmake-build-debug/librairie 9002 B C A Zoro
##          -  gcc src/librairie.c -g -o cmake-build-debug/librairie && cmake-build-debug/librairie 9003 Purge Ivan Porche Zoro A B C D
## nil: gcc src/nil.c -g -o cmake-build-debug/nil && cmake-build-debug/nil 9000 5 127.0.0.1 9001 ::1 9002 127.0.0.1 9003
## client: gcc src/client.c -o cmake-build-debug/client && cmake-build-debug/client localhost 9000 A Dune

## COMMANDE UTILES:
#     netstat -telpn
#     Addresse Turing: ssh nzoyemngueguin@turing.u-strasbg.fr