#!/bin/bash

## Création des libraries
cmake-build-debug/librairie 9001 Zoro A Dune Two > /dev/null &
sleep 1
cmake-build-debug/librairie 9002 B C A Zoro X Cmake > /dev/null &
sleep 1
cmake-build-debug/librairie 9003 Purge Ivan Porche Zoro A B C D > /dev/null &
sleep 1
cmake-build-debug/librairie 9004 > /dev/null &
sleep 1

## Lancement de Nil
cmake-build-debug/nil 9000 5 127.0.0.1 9001 ::1 9002 127.0.0.1 9003 ::1 9004 > /dev/null &
sleep 2

## Lancement des clients
cmake-build-debug/client localhost 9000 A Dune > /dev/null &
sleep .1
#cmake-build-debug/client ::1 9000 Zoro B Dune > /dev/null &
#sleep .1
#cmake-build-debug/client localhost 9000 Tor X > /dev/null &
#sleep .1
#cmake-build-debug/client 127.0.0.1 9000 A Cmake > /dev/null &
#sleep .1
cmake-build-debug/client localhost 9000 A Dune &
#sleep .1

## Effectuons els commandes
#cmake-build-debug/client ::1 9001 A Dune &
sleep 6

## Kill backgoround processes
trap 'kill -9 %1' 2
trap 'kill -9 %2' 2
trap 'kill -9 %3' 2
trap 'kill -9 %4' 2
trap 'kill -9 %5' 2
trap 'kill -9 %6' 2
trap 'kill -9 %7' 2
