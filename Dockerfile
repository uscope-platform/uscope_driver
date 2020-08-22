FROM ubuntu:focal

WORKDIR /driver
ENV TZ=Europe/Rome
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt-get update
RUN apt-get -y install build-essential cmake
COPY src ./src
COPY includes ./includes
COPY CMakeLists.txt ./CMakeLists.txt
RUN cmake ./
RUN cmake --build .
CMD ["/driver/uscope_driver", "--debug"]