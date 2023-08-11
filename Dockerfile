# This Dockerfile is used by GitHub to generate an Alpine Linux
# based container with the latest Mg.
#
# To run the containerized Mg and edit files in your $HOME, use
#
#    docker run -ti --rm -v $HOME:/root troglobit/mg:latest some-filename
#

FROM alpine:latest

RUN apk --no-cache add --virtual .build-dependencies \
  alpine-sdk autoconf automake git

WORKDIR /root
COPY . ./
RUN git clean -fdx
RUN ./autogen.sh
RUN ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var CFLAGS=-static
RUN make install-strip

FROM scratch
ARG revision=$revision
LABEL org.opencontainers.image.title="Mg"
LABEL org.opencontainers.image.description="Micro Emacs text editor"
LABEL org.opencontainers.image.documentation="https://man.troglobit.com/man1/mg.1.html"
LABEL org.opencontainers.image.url="https://github.com/troglobit/mg"
LABEL org.opencontainers.image.licenses="Unlicense"
LABEL org.opencontainers.image.revision="$revision"
LABEL org.opencontainers.image.source="https://github.com/troglobit/mg/tree/${revision:-master}/"

COPY --from=0 /usr/bin/mg /usr/bin/mg
COPY --from=0 /usr/share/doc/mg /usr/share/doc/mg

ENV WORKSPACE="/root"
WORKDIR "${WORKSPACE}"
ENTRYPOINT ["/usr/bin/mg"]
