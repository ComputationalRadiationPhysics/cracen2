#########################################################
# Dockerfile to build cracen2                           #
# Based on Ubuntu:trusty                                #
#########################################################

FROM ubuntu:trusty

MAINTAINER Fabian Jung

RUN apt-get update -y;\
	apt-get install software-properties-common -y;\
	add-apt-repository -y ppa:ubuntu-toolchain-r/test;\
	apt-get update -y

RUN apt-get install -y \
	build-essential \
	git \
	cmake3 \
	gcc-6 \
	g++-6 \
	python-dev \
	autotools-dev \
	libicu-dev \
	build-essential \
	libbz2-dev \
	libboost-all-dev \
	wget \
	nano

run git clone https://github.com/ComputationalRadiationPhysics/cracen2.git

run 	printf '#!/bin/bash\n\
	cd cracen2;\n\
	git pull;\n\
	mkdir -p build;\n\
	cd build;\n\
	cmake -DCMAKE_CXX_COMPILER=g++-6 ..;\n\
	make;\n\
	ctest;\n\
	make install' > makeTest.sh\
	chmod +x makeTest.sh

run	bash ./makeTest.sh

cmd	bash ./makeTest.sh


