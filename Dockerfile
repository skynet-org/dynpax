FROM archlinux AS builder

ENV CCACHE_DIR=/ccache

ENV CCACHE_MAXSIZE="5G"

RUN pacman -Sy --noconfirm && \
    pacman-key --init && \
    pacman-key --populate archlinux && \
    pacman -Syu --noconfirm

RUN pacman -S --noconfirm --needed \
    base-devel \
    cmake \
    ninja \
    ccache \
    git \
    tree \
    wget \
    unzip \
    tar

WORKDIR /src/dynpax

COPY CMakeLists.txt /src/dynpax/

COPY cmake/ /src/dynpax/cmake

RUN CC="/usr/bin/ccache /usr/bin/gcc" \
    CXX="/usr/bin/ccache /usr/bin/g++" \
    cmake -S . -B build_${TARGETARCH} -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DDEPENDENCIES_ONLY=ON \
    -DCMAKE_INSTALL_PREFIX=/opt/tmp/dynpax

RUN CC="/usr/bin/ccache /usr/bin/gcc" \
    CXX="/usr/bin/ccache /usr/bin/g++" \
    cmake --build build_${TARGETARCH} --parallel $(nproc)

COPY src src

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

FROM scratch AS runtime

COPY --from=builder /opt/dynpax /opt/dynpax

ENTRYPOINT [ "/opt/dynpax/bin/dynpax" ]
CMD [ "/opt/dynpax/bin/dynpax" ]
