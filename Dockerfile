FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    pkg-config \
    libopencv-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project

RUN mkdir -p TrabPratico lib

COPY TrabPratico/main.cpp TrabPratico/
COPY lib/vc.c lib/
COPY lib/vc.h lib/

RUN g++ -o trabpratico TrabPratico/main.cpp lib/vc.c -I lib $(pkg-config --cflags opencv4) $(pkg-config --libs opencv4) -lm

CMD ["./trabpratico"]