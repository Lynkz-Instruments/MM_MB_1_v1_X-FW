# Dockerfile for Cmake projects for STM32CubeMX
# Created by Étienne Machabée (etienne@lynkz.ca)
# Copyright Lynkz Instruments Inc, Amos 2025

FROM ubuntu:latest

ARG GCC_ARM_VER=gcc-arm-none-eabi-14.2.rel1
RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y libx11-6 libfreetype6 libxrender1 libfontconfig1 libxext6 xvfb curl wget bzip2 unzip git zip software-properties-common ninja-build cmake minicom gdb-multiarch locales && \
    echo "export LANG=C.UTF-8" >> ~/.bashrc && \
    echo "export LC_ALL=C.UTF-8" >> ~/.bashrc && \
    git config --global --add safe.directory '*'

# symlink libncurses and libncursesw 6 to 5. They should be ABI compatible.
RUN ln -s /usr/lib/x86_64-linux-gnu/libncursesw.so.6.4 /usr/lib/x86_64-linux-gnu/libncurses.so.5 && \
    ln -s /usr/lib/x86_64-linux-gnu/libncursesw.so.6.4 /usr/lib/x86_64-linux-gnu/libncursesw.so.5 && \
    ln -s /usr/lib/x86_64-linux-gnu/libtinfo.so.6.4 /usr/lib/x86_64-linux-gnu/libtinfo.so.5

RUN curl -L "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz" -o ${GCC_ARM_VER}.tar.xz && \
     mkdir ${GCC_ARM_VER} && tar -xJf ${GCC_ARM_VER}.tar.xz -C ${GCC_ARM_VER} --strip-components=1 && rm ${GCC_ARM_VER}.tar.xz

ENV PATH="/${GCC_ARM_VER}/bin:$PATH"
