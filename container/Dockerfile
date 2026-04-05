# C build environment — all packages from Debian 13 trixie
# GCC 14.2 | Python 3.13 | CMake 3.31 
FROM debian:trixie-slim
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Compiler & linker
    gcc \
    g++ \
    binutils \
    # Build systems
    make \
    ninja-build \
    cmake \
    # ARM Cortex-M / STM32 bare-metal toolchain
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libnewlib-dev \
    libstdc++-arm-none-eabi-newlib \
    # Python 3.13
    python3 \
    python3-pip \
    python3-venv \
    python3-kconfiglib \
    # SSH & network
    openssh-client \
    rsync \
    curl \
    wget \
    netcat-openbsd \
    iputils-ping \
    # Archive tools
    tar \
    gzip \
    bzip2 \
    xz-utils \
    unzip \
    zip \
    # Text & debug
    vim \
    nano \
    less \
    diffutils \
    gawk \
    # Process & system
    procps \
    htop \
    strace \
    # Common build utilities
    pkg-config \
    git \
    git-delta \
    ca-certificates \
    file \
    patch \
    gperf \
    ccache \
    # graphical tools for QM
    libgl1 \
    libglib2.0-0 \
    libxkbcommon0 \
    libqt6printsupport6 \
    libqt6widgets6 \
    libqt6svg6 \
    libqt6gui6 \
    libqt6core6 \
    libxcb-cursor0 \
    sudo \
    x11-apps \
    x11-utils \
    wayland-utils \
    qt6-wayland \
    libqt6waylandclient6 \
    weston \
    neovim \
    libwayland-bin \
    # QEMU for ARM/STM32 emulation
    qemu-system-arm \
    qemu-utils \
    # ST-Link flash tool for STM32
    stlink-tools \
    openocd \
    # Doxygen & PlantUML ---
    doxygen \
    graphviz \
    default-jre-headless \
    plantuml \
    openjdk-21-jdk \
    # X11 libraries for graphical apps mainly for plantUML
    xwayland \
    libxrender1 \
    libxtst6 \
    libxi6 \
    # Zsh & shell extras
    zsh \
    zsh-autosuggestions \
    zsh-syntax-highlighting \
    fzf \
    # Debugging
    gdb-multiarch \
    cgdb \
    tmux \
    # for viewing images, e.g. from QM screenshots
    shotwell \
    feh \ 
    # gosu — clean privilege drop from root to host user at runtime
    gosu \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# PlantUML helper script
RUN echo '#!/bin/bash' > /usr/local/bin/plantuml-cli && \
    echo 'exec java -jar /usr/share/plantuml/plantuml.jar "$@"' >> /usr/local/bin/plantuml-cli && \
    chmod +x /usr/local/bin/plantuml-cli

COPY opt /opt

COPY bin/ /usr/local/bin/
RUN chmod +x /usr/local/bin/*.sh


# Entrypoint: dynamically creates the host user/group at runtime and drops
# privileges with gosu. Must run as root, so no USER directive here.
COPY bin/ /usr/local/bin/
RUN chmod +x /usr/local/bin/*.sh

WORKDIR /isse

# Container starts as root; entrypoint.sh drops to the host user via gosu
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
