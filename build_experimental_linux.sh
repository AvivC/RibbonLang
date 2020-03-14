#!/bin/sh -x

clear
gcc `pkg-config --cflags gtk+-3.0` *.c -o plane.exe -g -Wall -Wno-unused `pkg-config --libs gtk+-3.0`

