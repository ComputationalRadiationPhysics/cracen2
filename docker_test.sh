#!/bin/bash

docker build -t cracen2 .
docker run -v `pwd`:/cracen2 cracen2
