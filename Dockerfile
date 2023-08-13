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
LABEL org.opencontainers.image.title         "Mg"
LABEL org.opencontainers.image.description   "Micro Emacs text editor from OpenBSD"
LABEL org.opencontainers.image.documentation "https://man.troglobit.com/man1/mg.1.html"
LABEL org.opencontainers.image.url           "https://github.com/troglobit/mg"
LABEL org.opencontainers.image.licenses      "Unlicense"

COPY --from=0 /usr/bin/mg /usr/bin/mg
COPY --from=0 /usr/share/doc/mg /usr/share/doc/mg

ENV WORKSPACE="/root"
WORKDIR "${WORKSPACE}"
ENTRYPOINT ["/usr/bin/mg"]
