FROM gcc:7.2

ADD . .
RUN ./configure && make && make install && make clean

ENTRYPOINT ["litmus"]
