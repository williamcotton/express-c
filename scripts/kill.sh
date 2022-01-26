#!/usr/bin/env bash

ps x | grep "build/$1$" | awk -v app=$1 '{
    if ($1 != "") {
        print "\nKilling process: "$1;
        system("kill -9 " $1);
        print("");
    }
}'
