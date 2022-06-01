## Описание проекта FRS

Face Recognition System (FRS) - это система распознавания лиц, которая используется для открывания дверей с установленными домофонами от компании "ЛАНТА" в Тамбове. FRS, по сути, является web-сервером. Для запросов используется RESTful web API.
Ниже описывается установка и настройка программы. В качестве примера используется свежеустановленная операционная система Ubuntu 20.04. Вы можете пропускать шаги, если требуемые зависимости уже установлены или использовать другие способы по установке пакетов.

## Установка драйверов NVIDIA
Команды взяты [отсюда](https://docs.nvidia.com/datacenter/tesla/tesla-installation-notes/index.html#ubuntu-lts).
```bash
$ sudo apt-get install linux-headers-$(uname -r)
$ distribution=$(. /etc/os-release;echo $ID$VERSION_ID | sed -e 's/\.//g')
$ wget https://developer.download.nvidia.com/compute/cuda/repos/$distribution/x86_64/cuda-$distribution.pin
$ sudo mv cuda-$distribution.pin /etc/apt/preferences.d/cuda-repository-pin-600
$ sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/$distribution/x86_64/7fa2af80.pub
$ echo "deb http://developer.download.nvidia.com/compute/cuda/repos/$distribution/x86_64 /" | sudo tee /etc/apt/sources.list.d/cuda.list
$ sudo apt-get update
$ sudo apt-get -y install cuda-drivers-470 --no-install-recommends
```
Перезагрузить систему.

## Установка зависимостей
```bash
$ sudo apt-get update
$ sudo apt-get --yes install ninja-build build-essential libssl-dev rapidjson-dev zlib1g-dev wget lsb-release software-properties-common git unzip
```

### Установка Clang
Для сборки проекта требуется Clang версии 14 или выше. Установка описана [здесь](https://apt.llvm.org/). Мы воспользуемся предлагаемым автоматическим скриптом:
```bash
$ sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
$ sudo ldconfig
```

### Сборка и установка CMake
Если есть установленная версия ниже 3.17.3, то потребуется удаление старой и сборка с установкой более свежей версии:
```bash
$ sudo apt purge --auto-remove cmake
$ cd ~
$ wget https://github.com/Kitware/CMake/releases/download/v3.23.1/cmake-3.23.1.tar.gz
$ tar -zxvf cmake-3.23.1.tar.gz
$ cd cmake-3.23.1
$ ./bootstrap -- -DCMAKE_BUILD_TYPE:STRING=Release
$ make -j`nproc`
$ sudo make install
```

### Сборка и установка libc++
За основу берём шаги из [документации](https://libcxx.llvm.org/BuildingLibcxx.html#the-default-build). В данном примере используется Clang версии 14. В вашем случае может быть более свежая версия.
```bash
$ cd ~
$ sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-14 30
$ sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-14 30
$ export CC=clang
$ export CXX=clang++
$ git clone https://github.com/llvm/llvm-project.git
$ cd llvm-project && mkdir build
$ cmake -G Ninja -S runtimes -B build -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" -DCMAKE_BUILD_TYPE=Release
$ ninja -C build cxx cxxabi unwind
$ sudo ninja -C build install-cxx install-cxxabi install-unwind
$ sudo ldconfig
$ export CXXFLAGS=-stdlib=libc++
```
**Важно!** Дальнейшая сборка должна идти с корректными значениями переменных окружения *CC*, *CXX*, *CXXFLAGS*. Если планируете использовать дополнительные консоли, то сначала необходимо выполнить команды:
```bash
$ export CC=clang
$ export CXX=clang++
$ export CXXFLAGS=-stdlib=libc++
```

### Сборка и установка Boost
```bash
$ cd ~
$ wget https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_79_0.zip
$ unzip boost_1_79_0.zip
$ cd boost_1_79_0/tools/build
$ ./bootstrap.sh
$ sudo ./b2 install --prefix=/usr/local
$ cd ~/boost_1_79_0
$ sudo b2 --build-dir=./build --with-filesystem --with-program_options --with-system --with-thread --with-date_time toolset=clang variant=release link=static runtime-link=static cxxflags="-std=c++20 -stdlib=libc++" linkflags="-stdlib=libc++" release install
```

### Сборка OpenCV
```bash
$ cd ~
$ wget https://github.com/opencv/opencv/archive/4.5.5.zip
$ unzip 4.5.5.zip
$ cd opencv-4.5.5 && mkdir build && cd build
$ cmake \
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
..
$ make -j`nproc`
```

### Сборка клиентских библиотек NVIDIA Triton™ Inference Server
```bash
$ cd ~
$ git clone https://github.com/triton-inference-server/client.git triton-client
$ cd triton-client
$ git checkout r22.04
$ mkdir build && cd build
$ cmake -DCMAKE_CXX_STANDARD=20 -DCMAKE_INSTALL_PREFIX=`pwd`/install -DCMAKE_BUILD_TYPE=Release -DTRITON_ENABLE_CC_HTTP=ON -DTRITON_ENABLE_CC_GRPC=OFF -DTRITON_ENABLE_PERF_ANALYZER=OFF -DTRITON_ENABLE_PYTHON_HTTP=OFF -DTRITON_ENABLE_PYTHON_GRPC=OFF -DTRITON_ENABLE_GPU=OFF -DTRITON_ENABLE_EXAMPLES=OFF -DTRITON_ENABLE_TESTS=OFF -DTRITON_COMMON_REPO_TAG=r22.04 -DTRITON_THIRD_PARTY_REPO_TAG=r22.04 ..
$ make cc-clients -j`nproc`
```

## Сборка FRS
```bash
$ cd ~
$ git clone --recurse-submodules https://github.com/rosteleset/frs.git
$ cd frs && mkdir build && cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR:PATH=$HOME/opencv-4.5.5/build -DTRITON_CLIENT_DIR:PATH=$HOME/triton-client/build/install -DCURL_ZLIB=OFF -DBoost_USE_STATIC_RUNTIME=ON ..
$ make -j`nproc`
```
**Примечание.** Возможна ситуация, что в системе установлен пакет OpenSSL версии 3 или выше (например, Ubuntu 22.04) и во время сборки появляется ошибка вида "OpenSSL version 1.x is required but version Y was found". В этом случае необходимо в файле *~/frs/contrib/mysql-connector-cpp/cdk/cmake/DepFindSSL.cmake* закомментировать строчки с проверкой версии OpenSLL :
```
#  if(NOT OPENSSL_VERSION_MAJOR EQUAL 1)
#    message(SEND_ERROR "OpenSSL version 1.x is required but version ${OPENSSL_VERSION} was found")
#  else()
#    message(STATUS "Using OpenSSL version: ${OPENSSL_VERSION}")
#  endif()
```
И повторить команду:
```bash
make -j`nproc`
```
После успешной сборки копируем содержимое директории ~/frs/opt в **/opt/frs**, делаем пользователя владельцем, созданный исполняемый файл **frs** копируем в **/opt/frs**:
```bash
$ sudo cp -R ~/frs/opt/ /opt/frs/
$ sudo chown $USER:$USER /opt/frs -R
$ cp ~/frs/build/frs /opt/frs
```

## Установка MySQL сервера
В качестве рабочей базы данных FRS использует MySQL.
```bash
$ sudo apt install mysql-server
```
В файле конфигурации */etc/mysql/mysql.conf.d/mysqld.cnf* снять комментарий для строчки с опцией **mysqlx-bind-address**
Также рекомендуем выключить *binary logging* и указать параметры в конце конфигурационного файла:
```
skip-log-bin
innodb_flush_log_at_trx_commit =  2
innodb_flush_log_at_timeout    = 10
```
Выполнить команды:
```bash
$ sudo service mysql start
$ sudo mysql_secure_installation
```
Далее нужно создать пользователя, например, user_frs и базу данных, например, db_frs; дать все права на чтение/запись.
Для создания структуры выполнить команду:
```bash
$ mysql -p -u user_frs db_frs < ~/frs/mysql/01_structure.sql
```
Для заполнения первоначальными данными:
```bash
$ mysql -p -u user_frs db_frs < ~/frs/mysql/02_default_data.sql
```

## NVIDIA Triton™ Inference Server
Для инференса нейронных сетей FRS использует Triton Inference Server (TIS) от компании NVIDIA. Существуют различные способы установки и применения TIS. В нашем случае мы используем контейнер. Команды для развертывания контейнера взяты [отсюда](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html#getting-started).
```bash
$ curl https://get.docker.com | sh \
  && sudo systemctl --now enable docker
$ distribution=$(. /etc/os-release;echo $ID$VERSION_ID) \
   && curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add - \
   && curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | sudo tee /etc/apt/sources.list.d/nvidia-docker.list
$ sudo apt-get update
$ sudo apt-get install -y nvidia-docker2
$ sudo systemctl restart docker
```
Для проверки, что CUDA контейнер работает, нужно запустить:
```bash
$ sudo docker run --rm --gpus all nvidia/cuda:11.0-base nvidia-smi
```
Получение контейнера TIS. В нашем случае мы используем версию 21.05. Вы можете использовать более свежую версию. Выбранная вами версия должна быть одинаковой для всех используемых контейнеров NVIDIA.
```bash
$ sudo docker pull nvcr.io/nvidia/tritonserver:21.05-py3
```

## Создание TensorRT планов моделей нейронных сетей
В TIS для инференса мы используем TensorRT планы, полученные из моделей нейронных сетей в формате ONNX (Open Neural Network Exchange). На данный момент FRS использует в работе три модели:
- **scrfd** - предназначена для поиска лиц на изображении. [Ссылка](https://github.com/deepinsight/insightface/tree/master/detection/scrfd) на проект. Путь к файлу: */opt/frs/plan_source/scrfd/scrfd_10g_bnkps.onnx*
- **genet** - предназначена для определения наличия на лице маски или тёмных очков. За основу взят [этот](https://github.com/idstcv/GPU-Efficient-Networks) проект. Модель получена путем "дообучения" (transfer learning with fine-tuning) на трех классах: открытое лицо, лицо в маске, лицо в тёмных очках. Путь к файлу: */opt/frs/plan_source/genet/genet_small_custom_ft.onnx*
- **arcface** - предназначена для вычисления биометрического шаблона лица. [Ссылка](https://github.com/deepinsight/insightface/tree/master/recognition/arcface_torch) на проект.  Ссылка для скачивания на [файл](https://drive.google.com/file/d/1gnt6P3jaiwfevV4hreWHPu0Mive5VRyP/view?usp=sharing) в формате ONNX.

Для создания файлов с TensorRT планами будем использовать контейнер и утилиту **trtexec**:
```bash
$ sudo docker pull nvcr.io/nvidia/tensorrt:21.05-py3
$ sudo docker run --gpus all -it --rm -v/opt/frs:/frs nvcr.io/nvidia/tensorrt:21.05-py3
```

Внутри контейнера:
```bash
# scrfd
$ mkdir -p /frs/model_repository/scrfd/1
$ trtexec --onnx=/frs/plan_source/scrfd/scrfd_10g_bnkps.onnx --saveEngine=/frs/model_repository/scrfd/1/model.plan --shapes=input.1:1x3x320x320 --workspace=1024

# genet
$ mkdir -p /frs/model_repository/genet/1
$ trtexec --onnx=/frs/plan_source/genet/genet_small_custom_ft.onnx --saveEngine=/frs/model_repository/genet/1/model.plan --workspace=3000

# arcface
$ mkdir -p /frs/model_repository/arcface/1
$ mkdir -p /frs/plan_source/arcface
#скачиваем файл glint360k_r50.onnx:
$ wget --load-cookies /tmp/cookies.txt "https://docs.google.com/uc?export=download&confirm=$(wget --quiet --save-cookies /tmp/cookies.txt --keep-session-cookies --no-check-certificate 'https://docs.google.com/uc?export=download&id=1gnt6P3jaiwfevV4hreWHPu0Mive5VRyP' -O- | sed -rn 's/.*confirm=([0-9A-Za-z_]+).*/\1\n/p')&id=1gnt6P3jaiwfevV4hreWHPu0Mive5VRyP" -O /frs/plan_source/arcface/glint360k_r50.onnx && rm -rf /tmp/cookies.txt
$ trtexec --onnx=/frs/plan_source/arcface/glint360k_r50.onnx --saveEngine=/frs/model_repository/arcface/1/model.plan --optShapes=input.1:1x3x112x112 --minShapes=input.1:1x3x112x112 --maxShapes=input.1:1x3x112x112 --workspace=1500
```
Закрываем контейнер (например, комбинация клавиш Ctrl+D). Делаем пользователя владельцем:
```bash
sudo chown $USER:$USER /opt/frs/plan_source/arcface -R
sudo chown $USER:$USER /opt/frs/model_repository -R
```

## Запуск контейнера Triton Inference Server
```bash
$ sudo docker run --gpus all -d -p8000:8000 -p8001:8001 -p8002:8002 -v/opt/frs/model_repository:/models nvcr.io/nvidia/tritonserver:21.05-py3 sh -c "tritonserver --model-repository=/models"
```
## Запуск FRS
Параметры конфигурации с описанием находятся в файле */opt/frs/sample.config*.
Запуск:
```bash
$ /opt/frs/run_frs
```

## Документация по API
Для генерации документации по API методам используется [apidoc](https://apidocjs.com).
```bash
$ cd ~/frs/apidoc
$ ./create_apidoc
$ ./create_apidoc_sg
```
Файлы появятся в директориях *~/frs/www/apidoc* и *~/frs/www/apidoc_sg*. Если их поместить в директории */opt/frs/static/apidoc* и */opt/frs/static/apidoc_sg* соответственно, то документация будет доступна по URL:
*http://localhost:9051/static/apidoc/*
*http://localhost:9051/static/apidoc_sg/*

## Советы по подбору GPU
- **Память GPU**
Чем больше памяти, тем больше моделей нейронных сетей могут быть использованы одновременно в TIS. Мы используем видеокарту с памятью 4 Гб .
- **GPU compute capability**
[Compute capability](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#compute-capability) определяет версию спецификации и указывает на вычислительные возможности GPU. Более подробную информацию можно посмотреть, например, в приведенной таблице [здесь](https://ru.wikipedia.org/wiki/CUDA).
Когда TensorRT создает план инференса модели (model.plan), то, чем выше compute capability, тем больше типов слоев нейронной сети будут обрабатываться на GPU.

С помощью утилиты **perf_analyzer** (поставляется с клиентскими библиотеками TIS, в данной инструкции установка не описана) мы проводили тесты инференса используемых в FRS моделей нейронных сетей в Yandex Cloud (Tesla V100, 32Gb, 5120 CUDA cores) и GTX 1650 (4Gb, 896 CUDA cores). Тесты показали, что V100 работает, примерно, в полтора раза быстрее. Но стоит она сильно больше. У V100 также есть преимущество по количеству памяти и в нее можно загрузить больше моделей. Значительная разница в количестве ядер CUDA (5120 против 896) не так существенно сказывается на производительности инференса. Что касается минимальных требований к FRS, то подойдет любая видео карта от GTX 1050 с памятью 4Гб и выше.

## История изменений
Историю изменений, начиная с версии 1.1.0, можно посмотреть [здесь](https://github.com/rosteleset/frs/blob/master/changes.md).
