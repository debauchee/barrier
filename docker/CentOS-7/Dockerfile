FROM centos:centos7

RUN yum -y update && yum -y upgrade
RUN yum -y install epel-release
RUN yum -y install git cmake3 make qt5-qtbase-devel gcc \
 avahi-compat-libdns_sd-devel gcc-c++ curl-devel \
 openssl-devel \
 libXtst-devel libXinerama-devel libICE-devel libXrandr-devel

RUN git clone https://github.com/debauchee/barrier/
RUN cd barrier && ./clean_build.sh | tee clean_build.log && tar czvf build.tgz build clean_build.log
