#!/bin/bash

LIB_NUMBER=$1
FILE="$2"

tail -n +2 "$FILE" > catalog0.csv
ROW_NUMBER=$(wc -l < catalog0.csv)
split -n l/$LIB_NUMBER -d --additional-suffix=".csv" catalog0.csv catalog
rm catalog0.csv
