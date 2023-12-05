# Ubuntu 22.04
# Building static FRS with Clang and libstdc++

FROM ubuntu:22.04 as builder
LABEL stage=builder

ARG LLVM_VERSION=15
ARG BOOST_VERSION=1.83.0
ARG BOOST_SUFFIX=1_83_0
ARG OPENCV_VERSION=4.8.0
ARG TRITON_VERSION=23.10

# Установка зависимостей
RUN apt-get update && apt-get --yes upgrade
RUN DEBIAN_FRONTEND=noninteractive \
    TZ=Etc/UTC \
    apt-get --yes install make lsb-release software-properties-common wget unzip git libssl-dev rapidjson-dev libz-dev cmake nasm

# Установка Clang
ENV PKG="clang-$LLVM_VERSION lldb-$LLVM_VERSION lld-$LLVM_VERSION clangd-$LLVM_VERSION"
RUN apt-get install --yes $PKG

ENV CC=clang-$LLVM_VERSION
ENV CXX=clang++-$LLVM_VERSION

# Сборка Boost
RUN cd ~ \
    && wget https://boostorg.jfrog.io/artifactory/main/release/$BOOST_VERSION/source/boost_$BOOST_SUFFIX.zip \
    && unzip boost_$BOOST_SUFFIX.zip \
    && cd boost_$BOOST_SUFFIX/tools/build \
    && ./bootstrap.sh --cxx=clang++-$LLVM_VERSION \
    && cd ~/boost_$BOOST_SUFFIX \
    && ~/boost_$BOOST_SUFFIX/tools/build/b2 --build-dir=./build --with-filesystem --with-program_options --with-system --with-thread --with-date_time toolset=clang-$LLVM_VERSION variant=release link=static runtime-link=static cxxflags="-std=c++20" release

# Сборка OpenCV
RUN cd ~ \
    && wget https://github.com/opencv/opencv/archive/$OPENCV_VERSION.zip \
    && unzip $OPENCV_VERSION.zip \
    && cd opencv-$OPENCV_VERSION && mkdir build && cd build \
    && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=20 \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_LIST="core,imgcodecs,calib3d,imgproc" \
        -DOPENCV_ENABLE_NONFREE=ON \
        -DBUILD_PERF_TESTS=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_ZLIB=ON \
        -DBUILD_OPENEXR=OFF \
        -DBUILD_JPEG=ON \
        -DBUILD_OPENJPEG=ON \
        -DBUILD_PNG=ON \
        -DBUILD_WEBP=ON \
        -DBUILD_PACKAGE=OFF \
        -DCMAKE_CONFIGURATION_TYPES="Release" \
        -DBUILD_JAVA=OFF \
        -DBUILD_opencv_python3=OFF \
        -DBUILD_opencv_python_bindings_generator=OFF \
        -DBUILD_opencv_python_tests=OFF \
        -DWITH_FFMPEG=OFF \
        -DWITH_GSTREAMER=OFF \
        -DWITH_GTK=OFF \
        -DWITH_OPENGL=OFF \
        -DWITH_1394=OFF \
        -DWITH_ADE=OFF \
        -DWITH_OPENEXR=OFF \
        -DWITH_PROTOBUF=OFF \
        -DWITH_QUIRC=OFF \
        -DWITH_TIFF=OFF \
        -DWITH_V4L=OFF \
        -DWITH_VA=OFF \
        -DWITH_VA_INTEL=OFF \
        -DWITH_VTK=OFF \
        -DWITH_OPENCL=OFF \
        .. \
    && make -j`nproc`

# Сборка клиентских библиотек NVIDIA Triton™ Inference Server
RUN cd ~ \
    && git clone https://github.com/triton-inference-server/client.git triton-client \
    && cd triton-client \
    && git checkout r$TRITON_VERSION \
    && mkdir build  \
    && cd build \
    && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_INSTALL_PREFIX:PATH=$HOME/triton-client/build/install \
        -DTRITON_ENABLE_CC_HTTP=ON \
        -DTRITON_ENABLE_CC_GRPC=OFF \
        -DTRITON_ENABLE_PERF_ANALYZER=OFF \
        -DTRITON_ENABLE_PYTHON_HTTP=OFF \
        -DTRITON_ENABLE_PYTHON_GRPC=OFF \
        -DTRITON_ENABLE_GPU=OFF \
        -DTRITON_ENABLE_EXAMPLES=OFF \
        -DTRITON_ENABLE_TESTS=OFF \
        -DTRITON_COMMON_REPO_TAG=r$TRITON_VERSION \
        -DTRITON_THIRD_PARTY_REPO_TAG=r$TRITON_VERSION \
        ..
RUN cd ~/triton-client/build \
    && make cc-clients -j`nproc`

# Копируем нужные для сборки FRS файлы
COPY ./*.cpp /root/frs/
COPY ./*.h /root/frs/
COPY ./CMakeLists.txt /root/frs/
COPY ./contrib/ /root/frs/contrib/

# Сборка FRS
RUN cd ~/frs \
    && mkdir build \
    && cd build \
    && cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBoost_USE_RELEASE_LIBS=ON \
        -DBoost_USE_STATIC_LIBS=ON \
        -DBoost_USE_STATIC_RUNTIME=OFF \
        -DBoost_NO_SYSTEM_PATHS=ON \
        -DBOOST_INCLUDEDIR:PATH=$HOME/boost_$BOOST_SUFFIX \
        -DBOOST_LIBRARYDIR:PATH=$HOME/boost_$BOOST_SUFFIX/stage/lib \
        -DOpenCV_DIR:PATH=$HOME/opencv-$OPENCV_VERSION/build \
        -DTRITON_CLIENT_DIR:PATH=$HOME/triton-client/build/install \
        -DCURL_ZLIB=OFF \
        -DWEB_STATIC_DIRECTORY:STRING="/opt/frs/static/" \
        .. \
    && make -j`nproc`


FROM ubuntu:22.04 as frs
COPY --from=builder /root/frs/build/frs /
ENTRYPOINT exec /frs --frs-config /opt/frs/frs.config 2>>/opt/frs/errors.txt
