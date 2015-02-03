sudo apt-get install -yq libdouble-conversion-dev libssl-dev make zip git autoconf libtool g++ libboost-all-dev libevent-dev flex bison libgoogle-glog-dev scons libkrb5-dev libsnappy-dev libsasl2-dev libnuma-dev

git clone https://github.com/gilgil1973/folly
cd folly/folly

autoreconf --install
./configure CXXFLAGS='-g -O0' CFLAGS='-g -O0' CPPFLAGS='-g -O0'
make -j8
cd ../..

autoreconf --install
CPPFLAGS=" -I`pwd`/folly/" LDFLAGS="-L`pwd`/folly/folly/.libs/" ./configure CXXFLAGS='-g -O0' CFLAGS='-g -O0' CPPFLAGS='-g -O0'
make -j8
