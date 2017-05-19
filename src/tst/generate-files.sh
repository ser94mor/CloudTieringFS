#!/bin/bash

TARGET_DIR="$1"
FILE_SIZE_B="$2"
FILE_NUMBER="$3"

for ((i=0; i<${FILE_NUMBER}; i++)); do
    dd if=/dev/urandom ibs=${FILE_SIZE_B} count=1 of="${TARGET_DIR}/file_num_${i}_size_${FILE_SIZE_B}"
done
