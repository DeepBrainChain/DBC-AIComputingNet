# dbc-node container-mode build image
#
# Ubuntu 22.04 base + Boost 1.66 from source + libcurl + libvirt-dev +
# websocketpp (header-only) + all the deps the existing dbc-node build needs.
# Most of the heavy 3rd-party libs (leveldb / secp256k1 / sr25519crust /
# cpp_substrate / tweetnacl / openssl) are already vendored inside the source
# tree at src/3rd/lib/linux, so we don't need to install Ubuntu equivalents
# for those.
#
# Build:    docker build -t dbc-node-builder -f Dockerfile .
# Compile:  docker run --rm -v $PWD/..:/src -w /src/src dbc-node-builder \
#               sh -c "cd .. && rm -rf build output && mkdir build && \
#                      cd build && cmake .. && make -j$(nproc) dbc"

FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

# Base build toolchain + system deps not in src/3rd/lib/linux.
RUN apt-get update && apt-get install --yes --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        wget \
        ca-certificates \
        git \
        unzip \
        tar \
        bzip2 \
        libssl-dev \
        libcurl4-openssl-dev \
        libvirt-dev \
        libxml2-dev \
        libreadline-dev \
        libncurses-dev \
        libcrypt-dev \
        libpcre3-dev \
        zlib1g-dev \
        uuid-dev \
        python3 \
    && rm -rf /var/lib/apt/lists/*

# Boost 1.66 from source.
# (Ubuntu 22.04 ships Boost 1.74, but dbc-node CMake hard-requires the
#  pre-1.69 boost::signals component, which 1.74 no longer provides.)
WORKDIR /opt
RUN wget --quiet https://archives.boost.io/release/1.66.0/source/boost_1_66_0.tar.bz2 \
 && echo "5721818253e6a0989583192f96782c4a98eb6204965316df9f5ad75819225ca9  boost_1_66_0.tar.bz2" | sha256sum -c \
 && tar -xjf boost_1_66_0.tar.bz2 \
 && rm boost_1_66_0.tar.bz2

WORKDIR /opt/boost_1_66_0
# In Boost 1.66, log_setup is built as part of the `log` component, not as a
# separate top-level library — bootstrap.sh rejects it as an unknown name.
RUN ./bootstrap.sh --prefix=/usr/local --with-libraries=signals,thread,system,filesystem,program_options,date_time,chrono,log,serialization,context,exception,test

# Boost 1.66's asio + GCC 11 dance: the consuming_buffers.hpp template gets
# parsed differently under C++17 / GCC 11. Apply the same workaround as the
# original Dockerfile.
RUN sed -i 's/result.count < result.max_buffers/(result.count) < result.max_buffers/g' \
        /opt/boost_1_66_0/boost/asio/detail/consuming_buffers.hpp \
 && ./b2 toolset=gcc \
        cxxflags="-std=c++17 -fPIC -Wno-deprecated-declarations -Wno-deprecated-copy" \
        cflags="-fPIC" \
        link=static runtime-link=shared \
        -j$(nproc) install \
 && ldconfig

# websocketpp 0.8.2 (header-only). dbc-node's ddn_ws_client uses it.
RUN wget --quiet -O /tmp/websocketpp.tar.gz \
        https://github.com/zaphoyd/websocketpp/archive/0.8.2.tar.gz \
 && tar -xzf /tmp/websocketpp.tar.gz -C /tmp \
 && cp -r /tmp/websocketpp-0.8.2/websocketpp /usr/local/include/ \
 && rm -rf /tmp/websocketpp*

# Build user / dir setup — running as root keeps things simple inside the
# container; the produced binary lives in /src/output owned by root and is
# fixed up post-build on the host if needed.
WORKDIR /src

CMD ["/bin/bash"]
