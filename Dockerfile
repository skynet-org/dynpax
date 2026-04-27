FROM ubuntu:26.04 AS builder

ENV CCACHE_DIR=/ccache

ENV CCACHE_MAXSIZE="5G"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    g++ \
    cmake \
    ninja-build \
    ccache \
    git \
    tree \
    wget \
    unzip \
    tar \
    patchelf \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src/dynpax

# caching hacks
COPY cmake/ /src/dynpax/cmake

RUN CC="/usr/bin/ccache /usr/bin/gcc" \
    CXX="/usr/bin/ccache /usr/bin/g++" \
    cmake -Scmake -B build_${TARGETARCH} -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DDEPENDENCIES_ONLY=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/tmp/dynpax

RUN CC="/usr/bin/ccache /usr/bin/gcc" \
    CXX="/usr/bin/ccache /usr/bin/g++" \
    cmake --build build_${TARGETARCH} --parallel $(nproc)

COPY CMakeLists.txt CMakeLists.txt
COPY src/ src/

RUN CC="/usr/bin/ccache /usr/bin/gcc" \
    CXX="/usr/bin/ccache /usr/bin/g++" \
    cmake -S . -B build_${TARGETARCH} -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/tmp/dynpax --fresh

RUN CC="/usr/bin/ccache /usr/bin/gcc" \
    CXX="/usr/bin/ccache /usr/bin/g++" \
    cmake --build build_${TARGETARCH} --target install --parallel $(nproc)

# IMPORTANT: This is required to populate the fakeroot with the correct dependencies
# we rely on dlopen so we need linker
RUN /opt/tmp/dynpax/bin/dynpax -t /opt/tmp/dynpax/bin/dynpax -f /opt/dynpax -i
RUN patchelf --set-interpreter /opt/dynpax/lib64/ld-linux-$(uname -m | tr '_' '-').so.2 /opt/dynpax/bin/dynpax
RUN patchelf --set-rpath "\$ORIGIN/../lib:\$ORIGIN/../usr/lib:\$ORIGIN/../usr/lib/$(uname -m)-linux-gnu" /opt/dynpax/bin/dynpax

FROM scratch AS runtime
COPY --from=builder /opt/dynpax /opt/dynpax
ENTRYPOINT [ "/opt/dynpax/bin/dynpax" ]
