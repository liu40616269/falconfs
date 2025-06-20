#!/bin/bash
set -euo pipefail

PREFIX=${1:-/usr/local/falconfs}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Using prefix: $PREFIX"
echo "Script directory: $SCRIPT_DIR"

export PATH="$PREFIX/bin":$PATH
export LD_LIBRARY_PATH="$PREFIX/lib64:$PREFIX/lib":$LD_LIBRARY_PATH

echo "$PREFIX/lib64" | tee /etc/ld.so.conf.d/00-falconfs.conf
echo "$PREFIX/lib" | tee -a /etc/ld.so.conf.d/00-falconfs.conf
ldconfig

# ----------------- gcc --------------------#
pushd .

GCC_VERSION=14.2.0
yum install -y gcc gcc-c++ glibc-static glibc-devel libmpc-devel tmux flex bison
wget https://github.com/gcc-mirror/gcc/archive/refs/tags/releases/gcc-$GCC_VERSION.tar.gz
tar -zxvf gcc-$GCC_VERSION.tar.gz
mv gcc-releases-gcc-$GCC_VERSION gcc-$GCC_VERSION
cd ./gcc-$GCC_VERSION/ && ./contrib/download_prerequisites
mkdir build && cd build
../configure --prefix=$PREFIX/gcc-$GCC_VERSION --enable-bootstrap --enable-threads=posix --enable--long-long --enable-checking=release --enable-language=c,c++ --disable-multilib
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/gcc-$GCC_VERSION $PREFIX

popd

# ----------------- cmake --------------------#
pushd .

CMAKE_VERSION=3.28.6
sudo yum install -y openssl-devel
wget https://cmake.org/files/v3.28/cmake-$CMAKE_VERSION.tar.gz
tar -xvf cmake-$CMAKE_VERSION.tar.gz
cd cmake-$CMAKE_VERSION
./bootstrap --prefix=$PREFIX/cmake-$CMAKE_VERSION
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/cmake-$CMAKE_VERSION $PREFIX

popd 

# ----------------- absl --------------------#
pushd .

ABSL_VERSION=20250512.1
wget https://github.com/abseil/abseil-cpp/releases/download/$ABSL_VERSION/abseil-cpp-$ABSL_VERSION.tar.gz
tar -zxvf abseil-cpp-$ABSL_VERSION.tar.gz
cd abseil-cpp-$ABSL_VERSION
cmake -B build -DCMAKE_INSTALL_PREFIX=$PREFIX/absl-$ABSL_VERSION -DBUILD_TESTING=ON
cmake --build build -j $(nproc)
cmake --install build

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/absl-$ABSL_VERSION $PREFIX

popd

# ----------------- protobuf --------------------#
pushd .

PROTOBUF_VERSION=3.17.3
wget https://github.com/protocolbuffers/protobuf/releases/download/v$PROTOBUF_VERSION/protobuf-cpp-$PROTOBUF_VERSION.tar.gz
tar -zxvf protobuf-cpp-$PROTOBUF_VERSION.tar.gz
cd protobuf-$PROTOBUF_VERSION
./configure --prefix=$PREFIX/protobuf-$PROTOBUF_VERSION
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/protobuf-$PROTOBUF_VERSION $PREFIX

popd

# ----------------- flatbuffer --------------------#
pushd .

FLATBUFFER_VERSION=25.2.10
wget https://github.com/google/flatbuffers/archive/refs/tags/v$FLATBUFFER_VERSION.tar.gz
tar -xvf v$FLATBUFFER_VERSION.tar.gz
cd flatbuffers-$FLATBUFFER_VERSION
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX/flatbuffers-$FLATBUFFER_VERSION
cmake --build build -j $(nproc)
cmake --install build

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/flatbuffers-$FLATBUFFER_VERSION $PREFIX

popd

# ----------------- brpc --------------------#
pushd .

#export LDFLAGS="-L$PREFIX/lib64 -labsl_base -labsl_log_severity -labsl_throw_delegate"
BRPC_VERSION=1.12.1
yum install -y gflags-devel leveldb leveldb-devel glog glog-devel libibverbs libibverbs-utils libibverbs-devel
wget https://github.com/apache/brpc/archive/refs/tags/$BRPC_VERSION.tar.gz
tar -zxvf $BRPC_VERSION.tar.gz
cd brpc-$BRPC_VERSION && mkdir build && cd build
cmake -DWITH_GLOG=ON -DWITH_RDMA=ON .. -DCMAKE_INSTALL_PREFIX=$PREFIX/brpc-$BRPC_VERSION -DCMAKE_PREFIX_PATH=$PREFIX
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/brpc-$BRPC_VERSION $PREFIX

popd

# ----------------- zookeeper --------------------#
pushd .

ZOOKEEPER_VERSION=3.9.3
yum install -y autoconf automake libtool libtool-ltdl-devel cppunit-devel maven java-1.8.0-openjdk-devel
wget https://github.com/apache/zookeeper/archive/refs/tags/release-$ZOOKEEPER_VERSION.tar.gz
tar -xvf release-$ZOOKEEPER_VERSION.tar.gz
cd zookeeper-release-$ZOOKEEPER_VERSION/zookeeper-jute
mvn clean install -DskipTests
cd ../zookeeper-client/zookeeper-client-c
mvn clean install -DskipTests
mkdir -p $PREFIX/zookeeper-$ZOOKEEPER_VERSION/include/zookeeper $PREFIX/zookeeper-$ZOOKEEPER_VERSION/lib64
cp -r generated/*.h $PREFIX/zookeeper-$ZOOKEEPER_VERSION/include/zookeeper
cp -r include/*.h $PREFIX/zookeeper-$ZOOKEEPER_VERSION/include/zookeeper
cp -d target/c/lib/libzookeeper* $PREFIX/zookeeper-$ZOOKEEPER_VERSION/lib64

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/zookeeper-$ZOOKEEPER_VERSION $PREFIX

popd

# ----------------- huaweicloud-sdk-c-obs --------------------#
pushd .

OBS_VERSION=3.24.12
wget https://github.com/huaweicloud/huaweicloud-sdk-c-obs/archive/refs/tags/v$OBS_VERSION.tar.gz
tar -zxvf v$OBS_VERSION.tar.gz
cd huaweicloud-sdk-c-obs-$OBS_VERSION
sed -i '/if(NOT DEFINED OPENSSL_INC_DIR)/,+5d' CMakeLists.txt
sed -i '/OPENSSL/d' CMakeLists.txt
cd source/eSDK_OBS_API/eSDK_OBS_API_C++ &&
    export SPDLOG_VERSION=spdlog-1.12.0 && bash build.sh sdk &&
    mkdir -p $PREFIX/obs-$OBS_VERSION &&
    tar zxvf sdk.tgz -C $PREFIX/obs-$OBS_VERSION &&
    rm $PREFIX/obs-$OBS_VERSION/lib/libcurl.so* &&
    rm $PREFIX/obs-$OBS_VERSION/lib/libssl.so* &&
    rm $PREFIX/obs-$OBS_VERSION/lib/libcrypto.so* &&
    rm -rf $PREFIX/obs-$OBS_VERSION/demo &&
    rm $PREFIX/obs-$OBS_VERSION/readme.txt
ln -s libiconv.so $PREFIX/obs-$OBS_VERSION/lib/libiconv.so.0

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/zookeeper-$ZOOKEEPER_VERSION $PREFIX
# compatible with legacy code
bash "$SCRIPT_DIR/link_to.sh" $PREFIX/zookeeper-$ZOOKEEPER_VERSION $PREFIX/obs

popd

# ----------------- jasncpp --------------------#
pushd .

JSONCPP_VERSION=1.9.6
wget https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/$JSONCPP_VERSION.tar.gz
tar -xzvf $JSONCPP_VERSION.tar.gz
cd jsoncpp-$JSONCPP_VERSION
sed -i 's/set(CMAKE_CXX_STANDARD 11)/set(CMAKE_CXX_STANDARD 17)/' CMakeLists.txt
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PREFIX/jsoncpp-$JSONCPP_VERSION
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/jsoncpp-$JSONCPP_VERSION $PREFIX

popd

# ----------------- gtest --------------------#
pushd .

GTEST_VERSION=1.17.0
wget https://github.com/google/googletest/releases/download/v$GTEST_VERSION/googletest-$GTEST_VERSION.tar.gz
tar -xvf googletest-$GTEST_VERSION.tar.gz
cd googletest-$GTEST_VERSION
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$PREFIX/googletest-$GTEST_VERSION
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/googletest-$GTEST_VERSION $PREFIX

popd

# ----------------- python --------------------#
pushd .

PYTHON_VERSION=3.11.13
wget https://www.python.org/ftp/python/$PYTHON_VERSION/Python-$PYTHON_VERSION.tar.xz
tar -xvf Python-$PYTHON_VERSION.tar.xz
cd Python-$PYTHON_VERSION
./configure --prefix=$PREFIX/python-$PYTHON_VERSION --enable-shared --enable-optimizations
make -j$(nproc)
make install

bash "$SCRIPT_DIR/link_to.sh" $PREFIX/python-$PYTHON_VERSION $PREFIX

popd

# ----------------- falconfs --------------------#
pushd .

yum install -y ninja-build readline-devel fuse fuse-devel fmt-devel

popd

