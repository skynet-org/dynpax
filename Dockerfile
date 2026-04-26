FROM alpine:3.23 AS builder

ENV CCACHE_DIR=/ccache

ENV CCACHE_MAXSIZE="5G"

RUN apk update && apk upgrade --no-cache && \
    apk add --no-cache --virtual=build-deps \
    gcc g++ git musl curl zip unzip tar wget \
    cmake ninja pkgconf make ccache \
    automake autoconf autoconf-archive tree \
    musl-dev libstdc++-dev ccache

WORKDIR /src/dynpax

COPY CMakeLists.txt /src/dynpax/

COPY cmake/ /src/dynpax/cmake

RUN CC="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-gcc" \
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
    -DDEPENDENCIES_ONLY=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/dynpax

RUN CC="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-gcc" \
    CXX="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-g++" \
    AR=/usr/bin/$(uname -m)-alpine-linux-musl-ar \
    NM=/usr/bin/$(uname -m)-alpine-linux-musl-nm \
    CPP=/usr/bin/$(uname -m)-alpine-linux-musl-c++ \
    RANLIB=/usr/bin/$(uname -m)-alpine-linux-musl-ranlib \
    CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake --build build_${TARGETARCH} --parallel $(nproc)

COPY . .

RUN CC="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-gcc" \
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
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/dynpax --fresh

RUN CC="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-gcc" \
    CXX="/usr/bin/ccache /usr/bin/$(uname -m)-alpine-linux-musl-g++" \
    AR=/usr/bin/$(uname -m)-alpine-linux-musl-ar \
    NM=/usr/bin/$(uname -m)-alpine-linux-musl-nm \
    CPP=/usr/bin/$(uname -m)-alpine-linux-musl-c++ \
    RANLIB=/usr/bin/$(uname -m)-alpine-linux-musl-ranlib \
    CFLAGS="-static -static-libgcc -flto=auto -fno-pie -no-pie" \
    CXXFLAGS="-static -static-libgcc -static-libstdc++ -fno-pie -no-pie -flto=auto" \
    LDFLAGS="-static -static-libgcc -static-libstdc++ -flto=auto -fno-pie -no-pie" \
    cmake --build build_${TARGETARCH} --target install --parallel $(nproc)

FROM scratch AS runtime

COPY --from=builder /opt/dynpax/bin/dynpax /opt/dynpax/bin/dynpax

CMD [ "/opt/dynpax/bin/dynpax" ]
