# build stage based on ubuntu LTS
FROM ubuntu@sha256:da6fc2be547864451aa253836dd926da33623312df4a9a243e35dc877c378a78 AS build

# install build dependencies without recommendations and clean apt cache in same layer
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -q -y --no-install-recommends \
        g++ make curl zip unzip tar binutils cmake git yasm ninja-build pkg-config \
        libssl-dev zlib1g-dev libpcre2-dev libuuid1 uuid-dev uuid-runtime ca-certificates && \
    rm -rf /var/lib/apt/lists/*

COPY ./ /build
WORKDIR /build
RUN cmake --preset linux-release -DBUILD_TESTING=OFF && \
    cmake --build --preset linux-release --target Server --parallel "$(nproc)"

# runtime stage – also based on ubuntu LTS; allow STEAM_APP_ID to be overridden
FROM ubuntu@sha256:da6fc2be547864451aa253836dd926da33623312df4a9a243e35dc877c378a78 AS runtime

# default Steam AppID can be overridden with --build-arg STEAM_APP_ID=xxxx
ARG STEAM_APP_ID=335300

# Numeric IDs avoid collisions with accounts already present in the base image.
ENV HOME=/home/rekindled
RUN mkdir -p /opt/rekindled-ds2s-server/Saved /home/rekindled \
    && chown 1000:1000 /opt/rekindled-ds2s-server/Saved /opt/rekindled-ds2s-server /home/rekindled \
    && chmod 755 /opt/rekindled-ds2s-server/Saved /opt/rekindled-ds2s-server /home/rekindled \
    && apt update \
    # Healthcheck needs curl to check the server
    && apt install -y --no-install-recommends --reinstall ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

# expose the various ports the game server uses so operators can easily publish them
#   50000/udp – DS3 game traffic (also used for quickmatch/arena)
#   50010/udp – DS2 game traffic
#   50050/udp – arena transport (DS3)
# and the HTTP/admin API port
EXPOSE 50000/udp 50010/udp 50050/udp
EXPOSE 50005/tcp

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
  CMD curl -fsS http://localhost:50005/ || exit 1

# write the AppID from build arg
ENV STEAM_APP_ID=${STEAM_APP_ID}
USER 1000:1000
RUN echo "$STEAM_APP_ID" > /opt/rekindled-ds2s-server/steam_appid.txt

# Copy only the built runtime outputs from the build stage into the runtime image.
# Avoid copying the full /build tree to keep image size small.
COPY --from=build --chown=1000:1000 /build/intermediate/cmake/linux-release/bin/Release/. /opt/rekindled-ds2s-server/

ENV LD_LIBRARY_PATH="/opt/rekindled-ds2s-server"

WORKDIR /opt/rekindled-ds2s-server
ENTRYPOINT ["/opt/rekindled-ds2s-server/Server"]
CMD [] 
