#!/bin/bash
set -e

# ── Validate required env vars ────────────────────────────────────────────────
: "${FIXUID:?ERROR: FIXUID env var not set. Pass -e FIXUID=\$(id -u) to docker run}"
: "${FIXGID:?ERROR: FIXGID env var not set. Pass -e FIXGID=\$(id -g) to docker run}"
: "${HOST_USER:?ERROR: HOST_USER env var not set. Pass -e HOST_USER=\$(whoami) to docker run}"

# ── Create group if it doesn't exist ─────────────────────────────────────────
if ! getent group "$FIXGID" > /dev/null 2>&1; then
    groupadd -g "$FIXGID" "$HOST_USER"
fi

# ── Create user if it doesn't exist ──────────────────────────────────────────
if ! getent passwd "$FIXUID" > /dev/null 2>&1; then
    useradd \
        -u "$FIXUID" \
        -g "$FIXGID" \
        -M \
        -s /usr/bin/zsh \
        "$HOST_USER"

    # Grant passwordless sudo (mirrors original behaviour)
    echo "$HOST_USER ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

    # The useradd -m flag copies /etc/skel (which already has .oh-my-zsh and
    # .zshrc baked in from the Dockerfile), so the shell is ready to go.
fi

# Resolve the actual home directory for this UID (handles pre-existing users)
USER_HOME=$(getent passwd "$FIXUID" | cut -d: -f6)

# ── Fix ownership of the workspace mount ─────────────────────────────────────
# /isse is mounted from the host; make sure our user can write to it.
chown "$FIXUID:$FIXGID" /isse 2>/dev/null || true

# ── Welcome banner ────────────────────────────────────────────────────────────
echo -e "\033[32m*** Welcome to isse container! ***\033[0m"
echo "  Running as: ${HOST_USER} (uid=${FIXUID} gid=${FIXGID})"

 
# 1. Redirige cachés de fuentes y aplicaciones
export XDG_CACHE_HOME=/tmp
# 2. Redirige preferencias y configuración de Java
export JAVA_OPTS="-Djava.util.prefs.userRoot=/tmp -Duser.home=/tmp"
# 3. Redirige la carpeta de archivos temporales estándar
export TMPDIR=/tmp
# 4. (Opcional) Variable específica que algunos wrappers de Java leen
export _JAVA_OPTIONS="-Djava.util.prefs.userRoot=/tmp -Duser.home=/tmp"


chown "$FIXUID:$FIXGID" "${USER_HOME}/.zshrc" "${USER_HOME}/.bashrc" 2>/dev/null || true
 
# ── Drop privileges and exec the requested command ───────────────────────────
 exec gosu "$FIXUID" "${@:-bash}"
