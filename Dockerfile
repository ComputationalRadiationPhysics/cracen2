#########################################################
# Dockerfile to build cracen2                           #
#########################################################

FROM ubuntu:xenial

MAINTAINER Fabian Jung

RUN apt-get update -y;\
	apt-get install software-properties-common -y;\
	add-apt-repository -y ppa:ubuntu-toolchain-r/test;\
	apt-get update -y

RUN	apt-get install -y \
	build-essential \
	gcc-6 \
	g++-6 \
	libboost-all-dev \
	wget

RUN	wget https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz; \
	tar -zxvf cmake-3.8.2.tar.gz; \
	cd cmake-3.8.2; \
	./bootstrap; \
	make -j8; \
	make install;

CMD	rm -rf build; \
	mkdir build; \
	cd build; \
	cmake -DCMAKE_CXX_COMPILER=g++-6 -DCMAKE_C_COMPILER=gcc-6 /cracen2 -DMPIEXEC_PREFLAGS=--allow-run-as-root && \
	make -j8 && \
	ctest;


