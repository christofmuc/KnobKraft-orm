FROM archlinux:base-devel AS dependencies

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN pacman -Syu --noconfirm \
    && pacman -S --needed --noconfirm \
        alsa-lib \
        cmake \
        curl \
        freetype2 \
        git \
        glew \
        gtk3 \
        icu \
        jack2 \
        libxext \
        mesa \
        pkgconf \
        python \
        tbb \
        webkit2gtk-4.1 \
    && pacman -Scc --noconfirm

FROM dependencies AS build

WORKDIR /KnobKraft-orm
COPY . .

RUN git config --global --add safe.directory /KnobKraft-orm \
    && git submodule update --recursive --init --depth 1

RUN cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=off \
    -DBUILD_PATCH_DATABASE_TESTS=ON \
    -S . \
    -B builds

RUN cmake --build builds --parallel "$(nproc)"

RUN cmake --build builds --target patch_database_migration_test --parallel "$(nproc)" \
    && ./builds/patch_database_migration_test

RUN test -x builds/The-Orm/KnobKraftOrm
