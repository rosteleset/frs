## Описание проекта FRS
Face Recognition System (FRS) - это система распознавания лиц, которая используется для открывания дверей с установленными домофонами от компании "ЛАНТА" в Тамбове. FRS, по сути, является web-сервером. Для запросов используется RESTful web API. Шаги для генерации документации по API методам описаны ниже в разделе "[Документация по API](#Документация-по-API)".
Для краткости, использование аббревиатуры GPU в нашем описании подразумевает графические процессоры компании NVIDIA. Для работы системы рекомендуется использовать GPU. Работа возможна в режиме CPU-only, но производительность будет значительно ниже.

---
### Содержание
 * [Общая схема взаимодействия с FRS](#scheme_frs)
 * [Специальные группы FRS](#sg_frs)
 * [Используемые модели нейронных сетей](#used_models)
 * [Установка и работа через Docker](#use_docker)
 * [Установка и настройка FRS](#setup_frs)
     * [Установка драйверов NVIDIA (если используется GPU)](#drivers_gpu)
     * [Установка и сборка зависимостей](#setup_deps)
         * [Установка Clang](#setup_clang)
         * [Сборка клиентских библиотек NVIDIA Triton™ Inference Server](#build_tis_client)
     * [Сборка FRS](#build_frs)
     * [Установка MySQL сервера](#setup_mysql)
     * [NVIDIA Triton™ Inference Server](#tis)
     * [Создание TensorRT планов моделей нейронных сетей (если используется GPU)](#make_trt_plans)
     * [Конфигурация моделей для TIS без использования GPU](#config_tis_cpu)
     * [Запуск контейнера Triton Inference Server с GPU](#run_gpu)
     * [Запуск контейнера Triton Inference Server без GPU](#run_cpu)
     * [Запуск FRS](#run_frs)
 * [Документация по API](#doc_api)
 * [Советы по подбору GPU](#advices_gpu)
---

<a name="scheme_frs"/>

## Общая схема взаимодействия с FRS
Первым делом, ваш бэкенд, с помощью вызова API метода **addStream**, регистрирует видео потоки. Основными параметрами метода являются:
- **streamId** - внутренний для бэкенда (внешний для FRS) идентификатор видео потока;
- **url** - это URL для захвата кадра с видео потока. FRS не декодирует видео, а работает с отдельными кадрами (скриншотами). Например, URL может выглядеть как ***http://<имя хоста>/cgi-bin/images_cgi?channel=0&user=admin&pwd=<пароль>***
- **callback** - это URL, который FRS будет использовать, когда распознает зарегистрированное лицо. Например, ***http://<адрес-бэкенда>/face_recognized?stream_id=1***

Для регистрации лиц используется метод **registerFace**. Основные параметры метода:
- **streamId** - идентификатор видео потока;
- **url** - это URL с изображением лица, которое нужно зарегистрировать. Например, ***http://<адрес-бэкенда>/image_to_register.jpg***
В случае успешной регистрации возвращается внутренний для FRS (внешний для бэкенда) уникальный идентификатор зарегистрированного лица - **faceId**.

Для старта и окончания обработки кадров используется метод **motionDetection**. Основная идея заключается в том, чтобы FRS обрабатывала кадры только тогда, когда зафиксировано движение перед домофоном и в кадре могут быть лица. Параметры метода:
- **streamId** - идентификатор видео потока;
- **start** - признак начала или окончания движения. Если **start="t"**, то FRS начинает через каждую секунду (задаётся параметром *delay-between-frames*) обрабатывать кадр с видео потока. Если **start="f"**, то FRS прекращает обработку.

Под обработкой кадра подразумевается следующая цепочка действий:
1. Поиск лиц с помощью нейронной сети *scrfd*.
2. Если лица найдены, то каждое проверяется на "размытость" и "фронтальность".
3. Если лицо не размыто (чёткое изображение) и фронтально, то с помощью нейронной сети *genet* определяется наличие маски и тёмные очков;
4. Для каждого лица без маски и без тёмных очков, с помощью нейронной сети *arcface*, вычисляется биометрический шаблон лица, он же дескриптор. Математически, дескриптор представляет собой 512-ти мерный вектор.
5. Далее каждый такой дескриптор попарно сравнивается с зарегистрированными в системе. Сравнение делается путём вычисления косинуса угла между векторами-дескрипторами: чем ближе значение к единице, тем больше похоже лицо на зарегистрированное. Если самое максимальное значение косинуса больше порогового значения (параметр *tolerance*), то лицо считается распознанным и FRS вызывает callback (событие распознавания лица) и в качестве параметров указывает идентификатор дескриптора (*faceId*) и внутренний для FRS (внешний для бэкенда) идентификатор события (*eventId*). Если в одном кадре окажется несколько распознанных лиц, то callback будет вызван для самого "качественного" ("лучшего") лица (параметр *blur*).
6. Каждое найденное неразмытое, фронтальное, без маски и без тёмных очков, лучшее лицо временно хранится в журнале FRS.

С помощью метода **bestQuality** у FRS можно запрашивать "лучший" кадр из журнала.  Например, незнакомый системе человек подошёл к домофону и открыл дверь ключом. Бэкенд знает время открытия ключом (date) и запрашивает лучший кадр у FRS. FRS ищет у себя в журнале кадр с максимальным *blur* из диапазона времени [date - best-quality-interval-before; date + best-quality-interval-after] и выдаёт его в качестве результата. Такой кадр - хороший кандидат для регистрации лица с помощью метода **registerFace**. Как правило, для хорошего распознавания необходимо зарегистрировать несколько лиц одного человека, в том числе с кадров, сделанных в тёмное время суток, когда камера переходит в инфракрасный режим работы.

<a name="sg_frs" />

## Специальные группы FRS
Специальная группа - это отдельная, изолированная и независимая от других группа лиц для распознавания (со своим *callback*) на всех зарегистрированных видео потоках. Из бэкенда, с помощью вызова методов addSpecialGroup, updateSpecialGroup, deleteSpecialGroup, можно создавать, изменять и удалять специальные группы. Методы с префиксом sg (sgRegisterFace, sgDeleteFaces, sgListFaces, sgUpdateGroup, sgRenewToken) могут быть использованы "третьей" стороной, поэтому в них используется авторизация по токену. Например, вы можете создать специальную группу для поиска людей и разрешить вызывать sg-методы не из вашего бэкенда.

<a name="used_models"/>

## Используемые модели нейронных сетей
На данный момент FRS использует в работе три модели:
- **scrfd** - предназначена для поиска лиц на изображении. [Ссылка](https://github.com/deepinsight/insightface/tree/master/detection/scrfd) на проект.
- **genet** - предназначена для определения наличия на лице маски или тёмных очков. За основу взят [этот](https://github.com/idstcv/GPU-Efficient-Networks) проект. Модель получена путем "дообучения" (transfer learning with fine-tuning) на трех классах: открытое лицо, лицо в маске, лицо в тёмных очках.
- **arcface** - предназначена для вычисления биометрического шаблона лица. [Ссылка](https://github.com/deepinsight/insightface/tree/master/recognition/arcface_torch) на проект.  Ссылка для скачивания на [файл](https://drive.google.com/file/d/1gnt6P3jaiwfevV4hreWHPu0Mive5VRyP/view?usp=sharing) в формате ONNX.

<a name="use_docker"/>

## Установка и работа через Docker
Для сборки и запуска FRS в Docker нужно следовать инструкциям в директории [docker](docker/readme.md) и пропустить следующий пункт "Установка и настройка FRS".

<a name="setup_frs"/>

## Установка и настройка FRS
Ниже описывается установка и настройка программы, если вы решили не использовать Docker. В качестве примера используется операционная система Ubuntu 22.04. Вы можете пропускать шаги, если требуемые зависимости уже установлены или использовать другие способы установки пакетов.

<a name="drivers_gpu"/>

### Установка драйверов NVIDIA (если используется GPU)
Команды взяты [отсюда](https://docs.nvidia.com/datacenter/tesla/tesla-installation-notes/index.html#ubuntu-lts).
```bash
$ sudo apt-get install linux-headers-$(uname -r)
$ distribution=$(. /etc/os-release;echo $ID$VERSION_ID | sed -e 's/\.//g')
$ wget https://developer.download.nvidia.com/compute/cuda/repos/$distribution/x86_64/cuda-keyring_1.0-1_all.deb
$ sudo dpkg -i cuda-keyring_1.0-1_all.deb
$ sudo apt-get update
$ sudo apt-get -y install cuda-drivers --no-install-recommends
```
Перезагрузить систему.

<a name="setup_deps"/>

### Установка и сборка зависимостей
```bash
$ sudo apt-get update
$ sudo apt-get --yes install make lsb-release software-properties-common wget unzip git libssl-dev rapidjson-dev libz-dev cmake libopencv-dev libboost-filesystem-dev libboost-program-options-dev libboost-date-time-dev libboost-system-dev libboost-thread-dev g++-12
```

<a name="setup_clang"/>

#### Установка Clang
Для сборки проекта требуется Clang версии 14 или выше. Если вы используете Ubuntu 20.04 или ниже, то также необходимо применять libc++ в качестве стандартной C++ библиотеки и за основу брать инструкции из предыдущих версий проекта (1.1.3).
```bash
$ export LLVM_VERSION=15
$ export PKG="clang-$LLVM_VERSION lldb-$LLVM_VERSION lld-$LLVM_VERSION clangd-$LLVM_VERSION"
$ sudo apt-get install --yes $PKG
$ export CC=clang-$LLVM_VERSION
$ export CXX=clang++-$LLVM_VERSION
```
**Важно!** Дальнейшая сборка должна идти с корректными значениями переменных окружения *CC*, *CXX*. Если планируете использовать дополнительные консоли, то сначала необходимо установить корректные значения указанных переменных.

<a name="build_tis_client"/>

#### Сборка клиентских библиотек NVIDIA Triton™ Inference Server
```bash
$ cd ~
$ git clone https://github.com/triton-inference-server/client.git triton-client
$ cd triton-client
$ export TRITON_VERSION=22.12
$ git checkout r$TRITON_VERSION
$ mkdir build && cd build
$ cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_INSTALL_PREFIX:PATH=~/triton-client/build/install \
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
$ make cc-clients -j`nproc`
```

<a name="build_frs"/>

### Сборка FRS
```bash
$ cd ~
$ git clone --recurse-submodules https://github.com/rosteleset/frs.git
$ cd frs && mkdir build && cd build
$ cmake \
-DCMAKE_BUILD_TYPE=Release \
-DTRITON_CLIENT_DIR:PATH=~/triton-client/build/install \
-DWEB_STATIC_DIRECTORY:STRING="/opt/frs/static/" \
-DCURL_ZLIB=OFF \
..
$ make -j`nproc`
```
После успешной сборки копируем содержимое директории ~/frs/opt в **/opt/frs**, делаем пользователя владельцем, созданный исполняемый файл **frs** копируем в **/opt/frs**:
```bash
$ sudo cp -R ~/frs/opt/ /opt/frs/
$ sudo chown $USER:$USER /opt/frs -R
$ cp ~/frs/build/frs /opt/frs
```

<a name="setup_mysql"/>

### Установка MySQL сервера
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

<a name="tis"/>

### NVIDIA Triton™ Inference Server
Для инференса нейронных сетей FRS использует Triton Inference Server (TIS) от компании NVIDIA. Существуют различные способы установки и применения TIS. В нашем случае мы используем контейнер. Команды для развертывания контейнера взяты [отсюда](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html#getting-started). Под пользователем root:
```bash
$ distribution=$(. /etc/os-release;echo $ID$VERSION_ID) \
    && curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg \
    && curl -s -L https://nvidia.github.io/libnvidia-container/$distribution/libnvidia-container.list | \
          sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
          tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
$ apt-get update
$ apt-get install -y nvidia-docker2
$ systemctl restart docker
```
Если используется GPU, то для проверки, что CUDA контейнер работает, нужно запустить:
```bash
$ sudo docker run --rm --gpus all nvidia/cuda:11.6.2-base-ubuntu20.04 nvidia-smi
```
Получение контейнера TIS. В нашем случае мы используем версию 22.12. Вы можете использовать более свежую версию. Выбранная вами версия должна быть одинаковой для всех используемых контейнеров NVIDIA.
```bash
$ sudo docker pull nvcr.io/nvidia/tritonserver:22.12-py3
```

<a name="make_trt_plans"/>

### Создание TensorRT планов моделей нейронных сетей (если используется GPU)
В TIS для инференса мы используем TensorRT планы, полученные из моделей нейронных сетей в формате ONNX (Open Neural Network Exchange).  Для создания файлов с TensorRT планами будем использовать контейнер и утилиту **trtexec**:
```bash
$ sudo docker run --gpus all -it --rm -v/opt/frs:/frs nvcr.io/nvidia/tritonserver:22.12-py3
```

Внутри контейнера:
```bash
# scrfd
$ mkdir -p /frs/model_repository/scrfd/1
$ /usr/src/tensorrt/bin/trtexec --onnx=/frs/plan_source/scrfd/scrfd_10g_bnkps.onnx --saveEngine=/frs/model_repository/scrfd/1/model.plan --shapes=input.1:1x3x320x320

# genet
$ mkdir -p /frs/model_repository/genet/1
$ /usr/src/tensorrt/bin/trtexec --onnx=/frs/plan_source/genet/genet_small_custom_ft.onnx --saveEngine=/frs/model_repository/genet/1/model.plan

# arcface
$ mkdir -p /frs/model_repository/arcface/1
$ mkdir -p /frs/plan_source/arcface
#скачиваем файл glint360k_r50.onnx:
$ wget --quiet https://nc2.lanta.me/s/HpjwAsoWr2XYek5/download -O /frs/plan_source/arcface/glint360k_r50.onnx
$ /usr/src/tensorrt/bin/trtexec --onnx=/frs/plan_source/arcface/glint360k_r50.onnx --saveEngine=/frs/model_repository/arcface/1/model.plan --shapes=input.1:1x3x112x112
```
Закрываем контейнер (например, комбинация клавиш Ctrl+D). Делаем пользователя владельцем:
```bash
sudo chown $USER:$USER /opt/frs/plan_source/arcface -R
sudo chown $USER:$USER /opt/frs/model_repository -R
```

<a name="config_tis_cpu"/>

### Конфигурация моделей для TIS без использования GPU
В этом случае будем использовать конфигурацию в директории */opt/frs/model_repository_onnx*
Нужно скопировать или переместить (как пожелаете) файлы:
 - *scrfd_10g_bnkps.onnx* в директорию */opt/frs/model_repository_onnx/scrfd/1/* под именем *model.onnx*
 - *genet_small_custom_ft.onnx* в директорию */opt/frs/model_repository_onnx/genet/1/* под именем *model.onnx*
 - скачанный по ссылке выше файл *glint360k_r50.onnx* в директорию */opt/frs/model_repository_onnx/arcface/1/* под именем *model.onnx*

<a name="run_gpu"/>

### Запуск контейнера Triton Inference Server с GPU
```bash
$ sudo docker run --gpus all -d --net=host -v /opt/frs/model_repository:/models nvcr.io/nvidia/tritonserver:22.12-py3 sh -c "tritonserver --model-repository=/models"
```

<a name="run_cpu"/>

### Запуск контейнера Triton Inference Server без GPU
```bash
$ sudo docker run -d --net=host -v /opt/frs/model_repository_onnx:/models nvcr.io/nvidia/tritonserver:22.12-py3 sh -c "tritonserver --model-repository=/models"
```

<a name="run_frs"/>

### Запуск FRS
Заполните параметры конфигурации вашими значениями в файле */opt/frs/sample.config*.
Запуск:
```bash
$ /opt/frs/run_frs
```
Рекомендуем создать systemd сервис для FRS.

<a name="doc_api"/>

## Документация по API
Для генерации документации по API методам используется [apidoc](https://apidocjs.com).
```bash
$ cd ~/frs/apidoc
$ ./create_apidoc
$ ./create_apidoc_sg
```
Файлы появятся в директориях *~/frs/www/apidoc* и *~/frs/www/apidoc_sg*. Если их поместить в директории */opt/frs/static/apidoc* и */opt/frs/static/apidoc_sg* соответственно, то документация будет доступна по URL:
*http://localhost:9051/static/apidoc/index.html*
*http://localhost:9051/static/apidoc_sg/index.html*

<a name="advices_gpu"/>

## Советы по подбору GPU
- **Память GPU**
Чем больше памяти, тем больше моделей нейронных сетей могут быть использованы одновременно в TIS. На данный момент мы используем видеокарту GTX 1650  с памятью 4 Гб .
- **GPU compute capability**
[Compute capability](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#compute-capability) определяет версию спецификации и указывает на вычислительные возможности GPU. Более подробную информацию можно посмотреть, например, в приведенной таблице [здесь](https://ru.wikipedia.org/wiki/CUDA).
Когда TensorRT создает план инференса модели (model.plan), то, чем выше compute capability, тем больше типов слоев нейронной сети будут обрабатываться на GPU.

С помощью утилиты **perf_analyzer** (поставляется с клиентскими библиотеками TIS, в данной инструкции установка не описана) мы проводили тесты инференса используемых в FRS моделей нейронных сетей в Yandex Cloud (Tesla V100, 32Gb, 5120 CUDA cores) и GTX 1650 (4Gb, 896 CUDA cores). Тесты показали, что V100 работает, примерно, в полтора раза быстрее. Но стоит она сильно больше. У V100 также есть преимущество по количеству памяти и в нее можно загрузить больше моделей. Значительная разница в количестве ядер CUDA (5120 против 896) не так существенно сказывается на производительности инференса. Что касается минимальных требований к FRS, то подойдет любая видео карта от GTX 1050 с памятью 4Гб и выше.
