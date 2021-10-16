# This Dockerfile is used by GitHub to generate an Alpine Linux
# based container with the latest Mg.
#
# To run the containerized Mg and edit files in your $HOME, use
#
#    docker run -ti -v $HOME:/root mg:latest some-filename
#

FROM alpine:latest

RUN apk --no-cache add --virtual .build-dependencies \
  alpine-sdk autoconf automake git

WORKDIR /root
COPY . ./
RUN git clean -fdx; ./autogen.sh && \
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var && make install-strip

FROM alpine:latest

COPY --from=0 /usr/bin/mg /usr/bin/mg
COPY --from=0 /usr/share/doc/mg /usr/share/doc/mg

ENV WORKSPACE="/root"
WORKDIR "${WORKSPACE}"
ENTRYPOINT ["/usr/bin/mg"]
