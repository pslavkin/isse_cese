#!/bin/bash
set -e

# Detect display server
if [ "$XDG_SESSION_TYPE" = "wayland" ] && [ -n "$WAYLAND_DISPLAY" ]; then
    echo "Detected: Wayland"
    chmod 755 "$XDG_RUNTIME_DIR"
    DISPLAY_ENV="- WAYLAND_DISPLAY=${WAYLAND_DISPLAY}
      - XDG_RUNTIME_DIR=/tmp/runtime
      - DISPLAY=${DISPLAY}
      - _JAVA_AWT_WM_NONREPARENTING=1
      - XAUTHORITY=/tmp/.Xauthority"
    DISPLAY_VOL="- ${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}:/tmp/runtime/${WAYLAND_DISPLAY}
      - /tmp/.X11-unix:/tmp/.X11-unix
      - ${XAUTHORITY:-${HOME}/.Xauthority}:/tmp/.Xauthority:ro"
elif [ -n "$DISPLAY" ]; then
    echo "Detected: X11"
    xhost +local:docker 2>/dev/null || true
    DISPLAY_ENV="- DISPLAY=${DISPLAY}"
    DISPLAY_VOL="- /tmp/.X11-unix:/tmp/.X11-unix:ro"
elif [ "$(uname)" = "Darwin" ]; then
    echo "Detected: macOS"
    xhost +local:docker 2>/dev/null || true
    DISPLAY_ENV="- DISPLAY=host.docker.internal:0"
    DISPLAY_VOL=""
else
    echo "WARNING: No display detected, GUI apps may not work"
    DISPLAY_ENV=""
    DISPLAY_VOL=""
fi

DOCKER_UID=$(id -u)
DOCKER_GID=$(id -g)
#set script dir as the actual dir if not $1 are passed
#$1 path could be absolute or relative
#if $1 is empty set as '.' to get the script dir
SCRIPT_DIR="${1:-.}"
echo "Using script directory: ${SCRIPT_DIR}"
HOST_USER=$(whoami)

cat > "${SCRIPT_DIR}/run.yml" << EOF
services:
  isse:
    image: pslavkin/isse:1.0
    pull_policy: if_not_present
    container_name: isse
    network_mode: host
    environment:
      ${DISPLAY_ENV}
      - HOST_USER=${HOST_USER}
      - FIXUID=${DOCKER_UID}
      - FIXGID=${DOCKER_GID}
      - QPC=/opt/qp-bundle/qpc
    volumes:
      - ${SCRIPT_DIR}:/isse
      - ~/.local:/home/${HOST_USER}/.local
      - ~/.gitconfig:/home/${HOST_USER}/.gitconfig:ro
      - ~/.ssh:/home/${HOST_USER}/.ssh:ro
      - /etc/resolv.conf:/etc/resolv.conf:ro
      ${DISPLAY_VOL}
    working_dir: /isse
    stdin_open: true
    tty: true
EOF

# Run with compose v2 or v1 fallback
if docker compose version &>/dev/null; then
    docker --log-level error compose -f "${SCRIPT_DIR}/run.yml" run --rm isse
elif command -v docker-compose &>/dev/null; then
    docker-compose -f "${SCRIPT_DIR}/run.yml" run --rm isse
else
    echo "ERROR: neither 'docker compose' nor 'docker-compose' found"
    echo "Install docker-compose-plugin: sudo apt install docker-compose-plugin"
    exit 1
fi
