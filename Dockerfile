FROM ubuntu:hirsute

# Install dependencies
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
  && apt-get -y install --no-install-recommends clang-format clang-tidy clang-tools clang clangd libc++-dev libc++1 libc++abi-dev \
  libc++abi1 libclang-dev libclang1 liblldb-dev libomp-dev libomp5 lld lldb llvm-dev llvm-runtime llvm python-clang libcurl4-openssl-dev \
  libblocksruntime-dev libkqueue-dev libpthread-workqueue-dev git build-essential python cmake ninja-build systemtap-sdt-dev libbsd-dev \
  linux-libc-dev apache2-utils fswatch uuid-dev valgrind ca-certificates wget curl xxd pkg-config libpq-dev libjansson-dev gpg-agent

RUN apt update && apt -y install --no-install-recommends software-properties-common && add-apt-repository ppa:ben-collins/libjwt

RUN apt-get install libjwt-dev

# Update certificates
RUN mkdir /usr/local/share/ca-certificates/cacert.org
RUN wget -P /usr/local/share/ca-certificates/cacert.org http://www.cacert.org/certs/root.crt http://www.cacert.org/certs/class3.crt
RUN update-ca-certificates
RUN git config --global http.sslCAinfo /etc/ssl/certs/ca-certificates.crt

# Build libdispatch
WORKDIR /deps
RUN git clone https://github.com/apple/swift-corelibs-libdispatch.git
WORKDIR /deps/swift-corelibs-libdispatch
RUN cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .
RUN ninja install
ENV LD_LIBRARY_PATH /usr/local/lib

# Download dbmate
RUN curl -fsSL -o /usr/local/bin/dbmate https://github.com/amacneil/dbmate/releases/latest/download/dbmate-linux-amd64
RUN chmod +x /usr/local/bin/dbmate

# Copy scripts
COPY scripts /usr/local/bin

WORKDIR /base

COPY . .

# Build and install
RUN make install
