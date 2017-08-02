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
	wget

run git clone https://github.com/ComputationalRadiationPhysics/cracen2.git

run cd cracen2;\
	git pull; \
	mkdir build;\
	cd build;\
	cmake -DCMAKE_CXX_COMPILER=g++-6 ..;\
	make; \
	ctest; \
	make install


