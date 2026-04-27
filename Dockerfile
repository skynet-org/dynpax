ARG TARGETARCH

FROM alpine:3.23 AS builder

ARG TARGETARCH
ARG BUILD_TESTING=OFF

ENV CCACHE_DIR=/ccache

ENV CCACHE_MAXSIZE="5G"

RUN apk update && apk upgrade --no-cache && \
    apk add --no-cache --virtual=build-deps \
    gcc g++ git musl curl zip unzip tar wget \
    cmake ninja pkgconf make ccache \
    automake autoconf autoconf-archive \
    musl-dev libstdc++-dev

WORKDIR /src/dynpax

COPY cmake/ /src/dynpax/cmake

RUN CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake -S cmake  -B build_${TARGETARCH} -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DDEPENDENCIES_ONLY=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/dynpax

RUN CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake --build build_${TARGETARCH} --parallel $(nproc)

COPY CMakeLists.txt CMakeLists.txt
COPY src src

RUN CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake -S . -B build_${TARGETARCH} -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DBUILD_TESTING=${BUILD_TESTING} \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/dynpax --fresh

RUN CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake --build build_${TARGETARCH} --target install --parallel $(nproc)

FROM scratch AS runtime

COPY --from=builder /opt/dynpax/bin/dynpax /opt/dynpax/bin/dynpax

ENTRYPOINT [ "/opt/dynpax/bin/dynpax" ]
CMD [ "--help" ]