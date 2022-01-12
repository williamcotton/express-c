# https://learnersguide.wordpress.com/2015/02/22/275/
start_cmd="build/$1 &"
ps | grep "build/$1$" | awk -v app=$1 '{
    if ($1 != "") {
        print "\nRestarting process: "$1;
        system("kill " $1);
        system("make $app");
        print("");
    }
}'
eval $start_cmd
