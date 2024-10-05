# docker build --platform=linux/amd64 -t lc .
# docker run --rm -it --platform=linux/amd64 lc
FROM ubuntu:noble

USER root
RUN DEBIAN_FRONTEND=noninteractive apt update && apt install -y git gcc-multilib build-essential tmux bsdmainutils file

USER ubuntu
WORKDIR /home/ubuntu/lc
COPY --chown=ubuntu:ubuntu tests/ tests/
COPY --chown=ubuntu:ubuntu Makefile lc.c gentok.py test.sh updatetoks.sh .
CMD make test && make
