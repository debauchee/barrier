FROM centos:centos6

RUN yum -y update && yum -y upgrade
RUN yum -y install epel-release
RUN yum -y install git cmake3 make qt5-qtbase-devel gcc \
 avahi-compat-libdns_sd-devel gcc-c++ curl-devel \
 openssl-devel \
 libXtst-devel libXinerama-devel libICE-devel libXrandr-devel

# c6 g++ is not CXX14
RUN yum -y install centos-release-scl-rh
# devtoolset-3 is enough but one can use devtoolset-N
RUN yum -y install devtoolset-3-gcc-c++.x86_64

RUN git clone https://github.com/debauchee/barrier/
RUN cd barrier && scl enable devtoolset-3 ./clean_build.sh | tee clean_build.log && tar czvf build.tgz build clean_build.log
