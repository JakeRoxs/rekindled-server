# build stage based on ubuntu LTS
FROM ubuntu@sha256:3131b4cc82a783df6c9df078f86e01819a13594b865c2cad47bd1bca2b7063bb AS build

# install build dependencies without recommendations and clean apt cache in same layer
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -q -y --no-install-recommends \
        g++ make curl zip unzip tar binutils cmake git yasm ninja-build pkg-config \
        libssl-dev zlib1g-dev libpcre3-dev libuuid1 uuid-dev uuid-runtime ca-certificates && \
    rm -rf /var/lib/apt/lists/*

COPY ./ /build
WORKDIR /build
RUN cmake --preset linux-release -DBUILD_TESTING=OFF && \
    cmake --build --preset linux-release --target Server --parallel "$(nproc)"

FROM steamcmd/steamcmd:latest@sha256:0e3dd116a002dfe756581e35ccf84591fc8b2bbd126a247f2c7de7061b901f23 AS steam

# Make steamcmd download steam client libraries so we can copy them later.
RUN steamcmd +login anonymous +quit

# runtime stage – also based on ubuntu LTS; allow STEAM_APP_ID to be overridden
FROM ubuntu@sha256:3131b4cc82a783df6c9df078f86e01819a13594b865c2cad47bd1bca2b7063bb AS runtime

# default Steam AppID can be overridden with --build-arg STEAM_APP_ID=xxxx
ARG STEAM_APP_ID=335300

RUN mkdir -p /opt/rekindled-ds2s-server/Saved \
    && if ! id rekindled >/dev/null 2>&1; then \
           useradd -r -s /bin/bash rekindled; \
       fi \
    && chown rekindled:rekindled /opt/rekindled-ds2s-server/Saved \
    && chown rekindled:rekindled /opt/rekindled-ds2s-server \
    && chmod 755 /opt/rekindled-ds2s-server/Saved \
    && chmod 755 /opt/rekindled-ds2s-server \
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
RUN echo "$STEAM_APP_ID" >> /opt/rekindled-ds2s-server/steam_appid.txt

# Copy only the built runtime outputs from the build stage into the runtime image.
# Avoid copying the full /build tree to keep image size small.
COPY --from=build /build/intermediate/cmake/linux-release/bin/Release/. /opt/rekindled-ds2s-server/

# Optional debug output during build (comment out in production):
# RUN ls -al /opt/rekindled-ds2s-server && find /opt/rekindled-ds2s-server -maxdepth 4 -type f -print

# Uncomment this to preserve full /build for inspection.
# COPY --from=build /build /build
COPY --from=steam /root/.local/share/Steam/steamcmd/linux64/steamclient.so /opt/rekindled-ds2s-server/steamclient.so

ENV LD_LIBRARY_PATH="/opt/rekindled-ds2s-server"

USER rekindled
WORKDIR /opt/rekindled-ds2s-server
ENTRYPOINT ["/opt/rekindled-ds2s-server/Server"]
CMD [] 
