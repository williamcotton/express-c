#!/usr/bin/env bash

cat demo/embeddedFiles.h | grep 'unsigned char' | awk '{print $3}' | sed 's/[\[\]]*//g' | awk '{printf("%s, ", $1)}' | sed 's/, $//' | awk '{
  print "unsigned char *embeddedFilesData[] = {" $0 "};"
}'

cat demo/embeddedFiles.h | grep 'unsigned int' | awk '{print $5}' | sed 's/[;]*//g' | awk '{printf("%s, ", $1)}' | sed 's/, $//' | awk '{
  print "int embeddedFilesLengths[] = {" $0 "};"
}'

cat demo/embeddedFiles.h | grep 'unsigned char' | awk '{print $3}' | sed 's/[\[\]]*//g' | awk '{printf("\"%s\", ", $1)}' | sed 's/, $//' | awk '{
  print "char *embeddedFilesNames[] = {" $0 "};"
}'

cat demo/embeddedFiles.h | grep 'unsigned int' | wc -l | awk '{
  print "const int embeddedFilesCount = " $0 ";"
}'

cat <<EOF
embedded_files_data_t embeddedFiles = {embeddedFilesData, embeddedFilesLengths, embeddedFilesNames, embeddedFilesCount};
EOF