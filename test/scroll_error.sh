#!/bin/sh

echo -ne '\033c'
echo -ne '\033[22;26r'
echo -ne '\033[20;1H'
echo "1"
echo "2"
echo "3"
echo "4"
echo "5"
echo "6"
echo "7"
sleep 3
echo -ne '\033[13;11r'
echo -ne '\033[20;1H'
echo "a1"
echo "a2"
echo "a3"
echo "a4"
echo "a5"
echo "a6"
echo "a7"
sleep 3
echo -ne '\033[30;32r'
echo -ne '\033[22;1H'
echo "b1"
echo "b2"
echo "b3"
echo "b4"
echo "b5"
echo "b6"
echo "b7"
sleep 3
echo -ne '\033c'
