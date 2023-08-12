# This Dockerfile is used by GitHub to generate an Alpine Linux
# based container with the latest Mg.
#
# To run the containerized Mg and edit files in your $HOME, use
#
#    docker run -ti --rm -v $HOME:/root troglobit/mg:latest some-filename
#

FROM alpine:latest
WORKDIR /root
COPY . ./

RUN apk --no-cache add --virtual .build-dependencies \
  alpine-sdk autoconf automake

RUN ./autogen.sh
RUN ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var CFLAGS=-static
RUN make install-strip

FROM scratch

COPY --from=0 /usr/bin/mg /usr/bin/mg
COPY --from=0 /usr/share/doc/mg /usr/share/doc/mg

ENV WORKSPACE="/root"
WORKDIR "${WORKSPACE}"
ENTRYPOINT ["/usr/bin/mg"]
