# https://www.kali.org/tools/slowhttptest/

slowhttptest -c 1000 -H -g -o audit/slowhttp -i 10 -r 100 -t GET -u http://localhost:3000 -x 24 -p 3
