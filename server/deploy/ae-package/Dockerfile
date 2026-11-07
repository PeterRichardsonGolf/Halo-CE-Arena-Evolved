# Arena Evolved dedicated server (preview): the image.
# Build it from the unpacked server package folder (the one holding halo, libSDL3.so.0, data/packs/):
#   docker build -t ae-server -f Dockerfile .      (podman build works the same, rootless too)
# The program and Arena Evolved's weapon data are in the image and read-only. Everything the server writes (settings, saves,
# logs, the admin password, the invite link) is in the volume /var/lib/ae-server. MCC is NOT in the image: mount your Steam
# library read-only (compose.yaml).
FROM docker.io/library/debian:12-slim

RUN useradd --system --uid 10001 --user-group --home-dir /var/lib/ae-server --no-create-home --shell /usr/sbin/nologin aeserver \
 && mkdir -p /opt/ae-server /var/lib/ae-server \
 && chown aeserver:aeserver /var/lib/ae-server

WORKDIR /opt/ae-server
COPY halo libSDL3.so.0 brokers.txt masters.txt container-start.sh ./
COPY data/packs ./packs
COPY gametypes ./gametypes
COPY licences ./licences
COPY rotation.txt ./rotation.default.txt
RUN chmod 0755 halo container-start.sh && chmod -R a+rX /opt/ae-server

ENV LD_LIBRARY_PATH=/opt/ae-server \
    HALO_DATA_ROOT=/var/lib/ae-server/data \
    HALO_SAVE_ROOT=/var/lib/ae-server/save \
    HALO_CONFIG_FILE=/var/lib/ae-server/config.toml \
    HALO_NET_BROKERS_FILE=/opt/ae-server/brokers.txt \
    HALO_SERVER_WEB_BIND=127.0.0.1 \
    HALO_NET_TUNNEL_PORT=2302 \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy

USER 10001:10001
VOLUME ["/var/lib/ae-server"]
# the internet port; the game's LAN ports and the web page are not published by default (compose.yaml)
EXPOSE 2302/udp
STOPSIGNAL SIGTERM
ENTRYPOINT ["/opt/ae-server/container-start.sh"]
