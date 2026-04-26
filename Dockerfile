# syntax=docker/dockerfile:1
FROM alpine:3.23 AS builder
ARG TARGETARCH

ENV CCACHE_DIR=/ccache_${TARGETARCH}
ENV CCACHE_MAXSIZE="5G"

RUN apk update && apk upgrade --no-cache && \
    apk add --no-cache --virtual=build-deps \
    gcc g++ git musl curl zip unzip tar wget \
    cmake ninja pkgconf make ccache \
    automake autoconf autoconf-archive tree \
    musl-dev libstdc++-dev ccache

COPY . /src/dynpax

WORKDIR /src/dynpax

RUN mkdir -p build_${TARGETARCH}

RUN --mount=type=cache,target=/src/dynpax/build_${TARGETARCH} \
    --mount=type=cache,target=/ccache_${TARGETARCH} \
    CC="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-gcc" \
    CXX="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-g++" \
    AR=/usr/bin/$(uname -m)-alpine-linux-musl-ar \
    NM=/usr/bin/$(uname -m)-alpine-linux-musl-nm \
    CPP=/usr/bin/$(uname -m)-alpine-linux-musl-c++ \
    RANLIB=/usr/bin/$(uname -m)-alpine-linux-musl-ranlib \
    CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake -S . -B build_${TARGETARCH} -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_INSTALL_PREFIX=/opt/dynpax --fresh && \
    cmake --build build_${TARGETARCH} --target install --parallel $(nproc)

FROM scratch AS runtime

COPY --from=builder /opt/dynpax/bin/dynpax /opt/dynpax/bin/dynpax

CMD [ "/opt/dynpax/bin/dynpax" ]
