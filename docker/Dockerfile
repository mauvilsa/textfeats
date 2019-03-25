FROM library/ubuntu:16.04

MAINTAINER Mauricio Villegas <mauricio_ville@yahoo.com>

ENV \
 LANG=en_US.UTF-8 \
 LC_ALL=C.UTF-8

RUN apt-get update --fix-missing \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      git \
      build-essential \
      cmake \
      pkg-config \
      libopencv-dev \
      libxml2-dev \
      libxslt1-dev \
      libconfig++-dev \
      wget

#ENV VER=6.8.2-10
ENV VER=6.8.9-10
#ENV VER=6.9.7-10

RUN cd /tmp \
 && wget https://imagemagick.org/download/releases/ImageMagick-$VER.tar.xz \
 && unxz ImageMagick-$VER.tar.xz \
 && tar xf ImageMagick-$VER.tar \
 && cd ImageMagick-$VER \
 && ./configure \
 && make -j$(nproc) \
 && make install \
 && ldconfig

RUN cd /tmp \
 && git clone --recursive https://github.com/mauvilsa/textfeats.git \
 && cd textfeats \
 && cmake . \
 && make -j$(nproc)

#RUN apt-get clean \
# && rm -rf /var/lib/apt/lists/*
