#!/bin/bash

echo "start to docker pull images"

cat ./image_lists| while read line
do
  echo "start to pull images:$line"
  docker pull $line
  echo -e
done
