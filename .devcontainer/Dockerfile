# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.208.0/containers/ubuntu/.devcontainer/base.Dockerfile

# [Choice] Ubuntu version (use hirsuite or bionic on local arm64/Apple Silicon): hirsute, focal, bionic
ARG VARIANT="hirsute"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

RUN wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
RUN sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt/ `lsb_release -cs`-pgdg main" >> /etc/apt/sources.list.d/pgdg.list'

# [Optional] Uncomment this section to install additional OS packages.
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends clang-format clang-tidy clang-tools clang clangd libc++-dev libc++1 libc++abi-dev \
    libc++abi1 libclang-dev libclang1 liblldb-dev libomp-dev libomp5 lld lldb llvm-dev llvm-runtime llvm python-clang libcurl4-openssl-dev \
    libblocksruntime-dev libkqueue-dev libpthread-workqueue-dev git build-essential python cmake ninja-build systemtap-sdt-dev libbsd-dev \
    linux-libc-dev apache2-utils fswatch uuid-dev valgrind xxd pkg-config libpq-dev postgresql-client-14 libjansson-dev gpg-agent

WORKDIR /vscode
RUN pwd
RUN git clone https://github.com/apple/swift-corelibs-libdispatch.git
WORKDIR /vscode/swift-corelibs-libdispatch
RUN ls -al
RUN cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ .
RUN ninja install
ENV LD_LIBRARY_PATH /usr/local/lib

RUN curl -fsSL -o /usr/local/bin/dbmate https://github.com/amacneil/dbmate/releases/latest/download/dbmate-linux-amd64
RUN chmod +x /usr/local/bin/dbmate
