#pragma once

#include <chrono>
#include <shared_mutex>
#include <filesystem>
#include <opencv2/opencv.hpp>

#include "concurrencpp/concurrencpp.h"
#include "mysqlx/xdevapi.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "absl/strings/substitute.h"

using time_point = std::chrono::time_point<std::chrono::steady_clock>;
using WriteLocker = std::scoped_lock<std::shared_timed_mutex>;
using ReadLocker = std::shared_lock<std::shared_timed_mutex>;
using String = std::string;
using Variant = std::variant<std::monostate, bool, int, double, String>;

template <typename Key, typename Value>
using HashMap = absl::flat_hash_map<Key, Value>;

template <typename T>
using HashSet = absl::flat_hash_set<T>;

using DateTime = absl::Time;

typedef cv::Mat FaceDescriptor;

struct ConfParam
{
  Variant value;  // значение параметра
  String comments;  // описание параметра
  Variant min_value;  // минимальное значение (если есть)
  Variant max_value;  // максимальное значение (если есть)
};

enum LogsLevel
{
  LOGS_ERROR [[maybe_unused]] = 0,
  LOGS_EVENTS,
  LOGS_VERBOSE
};

enum TaskType
{
  TASK_NONE,
  TASK_RECOGNIZE,
  TASK_REGISTER_DESCRIPTOR,
  TASK_PROCESS_FRAME,
  TASK_CHECK_MOTION,
  TASK_TEST
};

struct TaskData
{
  int id_vstream{};
  TaskType task_type{TASK_NONE};
  String frame_url{};
  double delay_s{};
  int error_count{};
  bool is_base64{};  //признак того, что URL изображения закодировано base64
  int id_sgroup{};

  TaskData(int id_vstream_, TaskType task_type_) : id_vstream(id_vstream_), task_type(task_type_) {}
};

enum DeliveryEventType
{
  FACE_RECOGNIZED = 1
};

enum DeliveryEventResult
{
  ERROR = 0,
  SUCCESSFUL = 1
};

enum FaceClassIndexes
{
  FACE_NONE = -1,
  FACE_NORMAL = 0,
};

struct RegisterDescriptorResponse
{
  int id_descriptor = 0;
  String comments;  //комментарии к выполнению запроса
  cv::Mat face_image;
  int face_left = 0;
  int face_top = 0;
  int face_width = 0;
  int face_height = 0;
};

struct ProcessFrameResult
{
  std::vector<int> id_descriptors;
};

struct TaskConfig
{
  HashMap<String, ConfParam> conf_params;  //общие конфигурационные параметры системы

  //явно указанные параметры видео потоков
  HashMap<String, int> conf_vstream_ext_to_id_vstream;  //связка внешнего идентификатора видео потока с внутренним
  HashMap<int, String> conf_id_vstream_to_vstream_ext;  //связка внутреннего идентификатора видео потока с внешним
  HashMap<int, String> conf_vstream_url;  //URL скриншотов с камер
  HashMap<int, String> conf_vstream_callback_url;  //URL для вызова при возникновении события распознавания лица
  HashMap<int, cv::Rect> conf_vstream_region;  //связка камеры и области, в которой определяются лица
  HashMap<int, HashMap<String, ConfParam>> vstream_conf_params;  //индивидуальные параметры для камер

  //параметры специальных групп
  static const int SG_MIN_DESCRIPTOR_COUNT = 1;  //минимально допустимое значение для параметра MAX_DESCRIPTOR_COUNT в API методах
  static const int DEFAULT_SG_MAX_DESCRIPTOR_COUNT = 1000;  //максимальное количество дескрипторов по-умолчанию в специальной группе
  HashMap<String, int> conf_token_to_sgroup;  //связка токена и идентификатора специальной группы
  HashMap<int, String> conf_sgroup_callback_url;  //URL для вызова при возникновении события распознавания лица в специальной группе
  HashMap<int, int> conf_sgroup_max_descriptor_count;  //максимальное количество дескрипторов в специальной группе
};

//для сбора статистики инференса
struct DNNStatsData
{
  int fd_count{};
  int fc_count{};
  int fr_count{};
};

//Константы с названием параметров конфигурации
const constexpr char* CONF_FILE = "file";  // = ".config";
const constexpr char* CONF_LOGS_LEVEL = "logs-level";  // = 2;  //уровень логов: 0 - ошибки; 1 - события; 2 - подробно
const constexpr char* CONF_FACE_CONFIDENCE_THRESHOLD = "face-confidence";  // = 0.8;  //порог вероятности определения лица
const constexpr char* CONF_FACE_CLASS_CONFIDENCE_THRESHOLD = "face-class-confidence";  // = 0.7;  //порог вероятности определения макси или темных очков на лице
const constexpr char* CONF_LAPLACIAN_THRESHOLD = "blur";  // = 50.0;  //нижний порог для проверки на размытость изображения (через дискретный аналог оператора Лапласа)
const constexpr char* CONF_LAPLACIAN_THRESHOLD_MAX = "blur-max";  // = 15000.0;  //верхний порог для проверки на размытость изображения (через дискретный аналог оператора Лапласа)
const constexpr char* CONF_TOLERANCE = "tolerance";  // = 0.7;  //толерантность - параметр, определяющий, что два вектора-дескриптора лица принадлежат одному человеку
const constexpr char* CONF_MAX_CAPTURE_ERROR_COUNT = "max-capture-error-count"; // = 3;  // Максимальное количество подряд полученных ошибок при получении кадра с камеры, после которого будет считаться попытка получения кадра неудачной
const constexpr char* CONF_RETRY_PAUSE = "retry-pause";  // = 30;  //пазуа в секундах перед следующей попыткой установки связи с видео потоком
const constexpr char* CONF_FACE_ENLARGE_SCALE = "face-enlarge-scale";  // = 1.4; //коэффициент расширения прямоугольной области лица для хранения
const constexpr char* CONF_ALPHA = "alpha";  // = 1.0;  //альфа-коррекция
const constexpr char* CONF_BETA = "beta";  // = 0;  //бета-коррекция
const constexpr char* CONF_GAMMA = "gamma";  // = 1.0;  //гамма-коррекция
const constexpr char* CONF_CALLBACK_TIMEOUT = "callback-timeout"; // = 2.0;  //время ожидания ответа при вызове callback URL в секундах
const constexpr char* CONF_CAPTURE_TIMEOUT = "capture-timeout"; // = 1.0;  //время ожидания захвата кадра с камеры в секундах
const constexpr char* CONF_MARGIN = "margin";  // = 5.0;  //отступ в процентах от краев кадра для уменьшения рабочей области
const constexpr char* CONF_DESCRIPTOR_INACTIVITY_PERIOD = "descriptor-inactivity-period";  // = 400;  //период неактивности дескриптора в днях, спустя который он удаляется
const constexpr char* CONF_BEST_QUALITY_INTERVAL_BEFORE = "best-quality-interval-before";  // = 5.0;  //период в секундах до временной точки для поиска лучшего кадра
const constexpr char* CONF_BEST_QUALITY_INTERVAL_AFTER = "best-quality-interval-after";  // = 2.0;  //период в секундах после временной точки для поиска лучшего кадра
const constexpr char* TYPE_BOOL = "bool";
const constexpr char* TYPE_INT = "int";
const constexpr char* TYPE_DOUBLE = "double";
const constexpr char* TYPE_STRING = "string";

//для детекции движения
const constexpr char* CONF_PROCESS_FRAMES_INTERVAL = "process-frames-interval";  // = 1.0  // интервал в секундах, втечение которого обрабатываются кадры после окончания детекции движения
const constexpr char* CONF_DELAY_BETWEEN_FRAMES = "delay-between-frames";  // = 1.0  // интервал в секундах между захватами кадров после детекции движения
const constexpr char* CONF_OPEN_DOOR_DURATION = "open-door-duration";   // = 5.0  // время открытия двери в секундах

//пороговые значения параметров roll, yaw, pitch, которые задают интервал в градусах, для определения фронтальности лица
const constexpr char* CONF_ROLL_THRESHOLD = "roll-threshold";  // = 45.0;  //интервал (-conf-roll_threshold; +conf_roll_threshold)
const constexpr char* CONF_YAW_THRESHOLD = "yaw-threshold";  // = 15.0;  //интервал (-conf-yaw_threshold; +conf_yaw_threshold)
const constexpr char* CONF_PITCH_THRESHOLD = "pitch-threshold";  // = 30.0;  //интервал (-conf-pitch_threshold; +conf_pitch_threshold)

//для периодического удаления устаревших логов из таблицы log_faces
const constexpr char* CONF_CLEAR_OLD_LOG_FACES = "clear-old-log-faces";  // = 8.0;  //период запуска очистки устаревших логов из таблицы log_faces
const constexpr char* CONF_LOG_FACES_LIVE_INTERVAL = "log-faces-live-interval";  // = 24.0;  //"время жизни" логов из таблицы log_faces

//для комментариев API методов
const constexpr char* CONF_COMMENTS_NO_FACES = "comments-no-faces";
const constexpr char* CONF_COMMENTS_PARTIAL_FACE = "comments-partial-face";
const constexpr char* CONF_COMMENTS_NON_FRONTAL_FACE = "comments-non-frontal-face";
const constexpr char* CONF_COMMENTS_NON_NORMAL_FACE_CLASS = "comments-non-normal-face-class";
const constexpr char* CONF_COMMENTS_BLURRY_FACE = "comments-blurry-face";
const constexpr char* CONF_COMMENTS_NEW_DESCRIPTOR = "comments-new-descriptor";
const constexpr char* CONF_COMMENTS_DESCRIPTOR_EXISTS = "comments-descriptor-exists";
const constexpr char* CONF_COMMENTS_DESCRIPTOR_CREATION_ERROR = "comments-descriptor-creation-error";
const constexpr char* CONF_COMMENTS_INFERENCE_ERROR = "comments-inference-error";
const constexpr char* CONF_COMMENTS_URL_IMAGE_ERROR = "comments-url-image-error";

//для инференса детекции лиц
const constexpr char* CONF_DNN_FD_INFERENCE_SERVER = "dnn-fd-inference-server";  //сервер
const constexpr char* CONF_DNN_FD_MODEL_NAME = "dnn-fd-model-name";  //название модели нейронной сети
const constexpr char* CONF_DNN_FD_INPUT_TENSOR_NAME = "dnn-fd-input-tensor-name";  //название входного тензора
const constexpr char* CONF_DNN_FD_INPUT_WIDTH = "dnn-fd-input-width";  //ширина изображения
const constexpr char* CONF_DNN_FD_INPUT_HEIGHT = "dnn-fd-input-height";  //высота изображения

//для инференса получения класса лица (нормальное, в маске, без маски)
const constexpr char* CONF_DNN_FC_INFERENCE_SERVER = "dnn-fc-inference-server";  //сервер
const constexpr char* CONF_DNN_FC_MODEL_NAME = "dnn-fc-model-name";  //название модели нейронной сети
const constexpr char* CONF_DNN_FC_INPUT_TENSOR_NAME = "dnn-fc-input-tensor-name";  //название входного тензора
const constexpr char* CONF_DNN_FC_INPUT_WIDTH = "dnn-fc-input-width";  //ширина изображения
const constexpr char* CONF_DNN_FC_INPUT_HEIGHT = "dnn-fc-input-height";  //высота изображения
const constexpr char* CONF_DNN_FC_OUTPUT_TENSOR_NAME = "dnn-fc-output-tensor-name";  //название выходного тензора
const constexpr char* CONF_DNN_FC_OUTPUT_SIZE = "dnn-fc-output-size";  //размер массива результата инференса

//для инференса получения дескриптора лица
const constexpr char* CONF_DNN_FR_INFERENCE_SERVER = "dnn-fr-inference-server";  //сервер
const constexpr char* CONF_DNN_FR_MODEL_NAME = "dnn-fr-model-name";  //название модели нейронной сети
const constexpr char* CONF_DNN_FR_INPUT_TENSOR_NAME = "dnn-fr-input-tensor-name";  //название входного тензора
const constexpr char* CONF_DNN_FR_INPUT_WIDTH = "dnn-fr-input-width";  //ширина изображения
const constexpr char* CONF_DNN_FR_INPUT_HEIGHT = "dnn-fr-input-height";  //высота изображения
const constexpr char* CONF_DNN_FR_OUTPUT_TENSOR_NAME = "dnn-fr-output-tensor-name";  //название выходного тензора
const constexpr char* CONF_DNN_FR_OUTPUT_SIZE = "dnn-fr-output-size";  //размер массива результата инференса

//для копирования данных событий
const constexpr char* CONF_COPY_EVENT_DATA = "flag-copy-event-data";  //флаг копирования данных события в отдельную директорию

//различного рода сообщения
inline constexpr absl::string_view ERROR_SQL_EXEC = "Ошибка: не удалось выполнить запрос $0.";
inline constexpr absl::string_view ERROR_SQL_EXEC_IN_FUNCTION = "Ошибка: не удалось выполнить запрос $0 в функции $1.";

//SQL запросы
const constexpr char* SQL_GET_COMMON_SETTINGS = "select *, unix_timestamp(time_point) unix_time_point from common_settings";

/*const constexpr char* SQL_UPDATE_COMMON_SETTINGS = R"_SQL_(
  update
    common_settings cs
  set
    cs.param_value = ?,
    cs.time_point = from_unixtime(?)
  where
    cs.param_name = ?
)_SQL_";*/

const constexpr char* SQL_GET_VIDEO_STREAMS = R"_SQL_(
  select
    vs.*
  from
    link_worker_vstream lw
    inner join video_streams vs
      on vs.id_vstream = lw.id_vstream
      and lw.id_worker = ?
)_SQL_";

const constexpr char* SQL_GET_VIDEO_STREAM_BY_EXT = "select * from video_streams where vstream_ext = ?";

const constexpr char* SQL_GET_VIDEO_STREAM_SETTINGS = R"_SQL_(
  select
    vss.*
  from
    link_worker_vstream lw
    inner join video_stream_settings vss
      on vss.id_vstream = lw.id_vstream
      and lw.id_worker = ?
)_SQL_";

const constexpr char* SQL_SET_COMMON_PARAM = R"_SQL_(
  update
    common_settings
  set
    param_value = ?
  where
    param_name = ?
)_SQL_";

const constexpr char* SQL_SET_VIDEO_STREAM_PARAM = R"_SQL_(
  insert into
    video_stream_settings
  set
    id_vstream = ?,
    param_name = ?,
    param_value = ?
  on duplicate key update
    param_value = ?
)_SQL_";

const constexpr char* SQL_ADD_VSTREAM = R"_SQL_(
  insert into
    video_streams
  set
    url = ?,
    callback_url = ?,
    region_x = ?,
    region_y = ?,
    region_width = ?,
    region_height = ?,
    vstream_ext = ?
)_SQL_";

const constexpr char* SQL_UPDATE_VSTREAM = R"_SQL_(
  update
    video_streams vs
  set
    url = ?,
    callback_url = ?,
    vs.region_x = ?,
    vs.region_y = ?,
    vs.region_width = ?,
    vs.region_height = ?
  where
    vs.id_vstream = ?
 )_SQL_";

/*const constexpr char* SQL_REMOVE_VSTREAM = R"_SQL_(
  delete from
    video_streams vs
  where
    vs.id_vstream = ?
)_SQL_";*/

const constexpr char* SQL_GET_FACE_DESCRIPTORS = R"_SQL_(
  select distinct
    fd.*
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = ?
    inner join face_descriptors fd
      on fd.id_descriptor = ld.id_descriptor

  union

  select distinct
    fd2.*
  from
    link_descriptor_sgroup ls
    inner join face_descriptors fd2
      on ls.id_descriptor = fd2.id_descriptor
)_SQL_";

const constexpr char* SQL_GET_FACE_DESCRIPTOR_BY_ID = "select * from face_descriptors where id_descriptor = ?";
const constexpr char* SQL_GET_ALL_FACE_DESCRIPTORS = "select id_descriptor from face_descriptors where id_descriptor not in (select ldsg.id_descriptor from link_descriptor_sgroup ldsg)";

const constexpr char* SQL_ADD_FACE_DESCRIPTOR = R"_SQL_(
  insert into
    face_descriptors
  set
    descriptor_data = ?
  )_SQL_";

/*const constexpr char* SQL_UPDATE_FACE_DESCRIPTOR = R"_SQL_(
  update
    face_descriptors
  set
    date_last = current_stamp
  where
    id_descriptor = ?
)_SQL_";*/

const constexpr char* SQL_REMOVE_FACE_DESCRIPTOR = "delete from face_descriptors fd where fd.id_descriptor = ?";

const constexpr char* SQL_ADD_DESCRIPTOR_IMAGE = R"_SQL_(
  insert into
    descriptor_images
  set
    id_descriptor = ?,
    mime_type = ?,
    face_image = ?
)_SQL_";

const constexpr char* SQL_GET_LINKS_DESCRIPTOR_VSTREAM = R"_SQL_(
  select
    ld.*
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = ?
)_SQL_";

const constexpr char* SQL_GET_LINKS_DESCRIPTOR_VSTREAM_BY_ID = R"_SQL_(
  select
    ld.id_descriptor
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = ?
      and ld.id_vstream = ?
)_SQL_";

const constexpr char* SQL_CHECK_LINKS_DESCRIPTOR_VSTREAM = R"_SQL_(
  select
    ld.id_descriptor
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = ?
      and ld.id_descriptor = ?
  limit 1
)_SQL_";

const constexpr char* SQL_ADD_LINK_DESCRIPTOR_VSTREAM =
  "insert ignore link_descriptor_vstream set id_descriptor = ?, id_vstream = ?";

const constexpr char* SQL_REMOVE_LINK_DESCRIPTOR_VSTREAM =
  "delete from link_descriptor_vstream where id_descriptor = ? and id_vstream = ?";

//получение дескрипторов, которые привязаны только к одному указанному видео потоку
const constexpr char* SQL_GET_SOLE_LINK_DESCRIPTORS_VSTREAM = R"_SQL_(
  select
    ldv.id_descriptor
  from
    link_descriptor_vstream ldv
  where
    ldv.id_vstream = ?
    and not exists(select * from link_descriptor_vstream where id_vstream != ? and id_descriptor = ldv.id_descriptor)
)_SQL_";

const constexpr char* SQL_ADD_LOG_FACE = R"_SQL_(
  insert into
    log_faces
  set
    id_vstream = ?,
    log_date = ?,
    id_descriptor = ?,
    quality = ?,
    screenshot = ?,
    face_left = ?,
    face_top = ?,
    face_width = ?,
    face_height = ?
)_SQL_";

const constexpr char* SQL_GET_LOG_FACE_BEST_QUALITY = R"_SQL_(
  select
    l.screenshot,
    l.face_left,
    l.face_top,
    l.face_width,
    l.face_height,
    cast(l.log_date as char) log_date
  from
    log_faces l
  where
    l.id_vstream = ?
    and (l.log_date >= ? - interval ? second) and (l.log_date <= ? + interval ? second)
  order by
    l.quality desc
  limit
    1
)_SQL_";

const constexpr char* SQL_GET_LOG_FACE_BY_ID = R"_SQL_(
  select
    l.screenshot,
    l.face_left,
    l.face_top,
    l.face_width,
    l.face_height,
    cast(l.log_date as char) log_date
  from
    log_faces l
  where
    l.id_log = ?
)_SQL_";

const constexpr char* SQL_GET_LOG_FACES_FROM_INTERVAL = R"_SQL_(
  select
    cast(l.log_date as char) log_date,
    l.id_descriptor,
    l.quality,
    l.screenshot,
    l.face_left,
    l.face_top,
    l.face_width,
    l.face_height
  from
    log_faces l
  where
    l.id_vstream = ?
    and l.log_date >= ?
    and l.log_date <= ?
  order by
    l.log_date
)_SQL_";

const constexpr char* SQL_REMOVE_OLD_LOG_FACES = "delete from log_faces where log_date < ?";

const constexpr char* SQL_ADD_LOG_DELIVERY_EVENT = R"_SQL_(
  insert ignore into
    log_delivery_events
  set
    delivery_type = ?,
    delivery_result = ?,
    id_vstream = ?,
    id_descriptor = ?
)_SQL_";

const constexpr char* SQL_GET_WORKER_BY_ID = "select id_worker from workers where id_worker = ?";

const constexpr char* SQL_ADD_LINK_WORKER_VSTREAM = R"_SQL_(
  insert into
    link_worker_vstream
  set
    id_worker = ?,
    id_vstream = ?
  on duplicate key update
    id_worker = ?
)_SQL_";

const constexpr char* SQL_REMOVE_LINK_WORKER_VSTREAM = R"_SQL_(
  delete from
    link_worker_vstream
  where
    id_worker = ?
    and id_vstream = ?
)_SQL_";

const constexpr char* SQL_GET_SPECIAL_GROUPS = "select * from special_groups";
const constexpr char* SQL_GET_SGROUP_BY_ID = "select * from special_groups where id_special_group = ?";
const constexpr char* SQL_GET_SGROUP_BY_NAME = "select id_special_group from special_groups where group_name = ?";
const constexpr char* SQL_GET_LINKS_DESCRIPTOR_SGROUP = "select * from link_descriptor_sgroup";

const constexpr char* SQL_ADD_SPECIAL_GROUP = R"_SQL_(
  insert into
    special_groups
  set
    group_name = ?,
    sg_api_token = ?,
    max_descriptor_count = ?
)_SQL_";

const constexpr char* SQL_UPDATE_SPECIAL_GROUP = "update special_groups set group_name = ?, max_descriptor_count = ? where id_special_group = ?";
const constexpr char* SQL_DELETE_SPECIAL_GROUP = "delete from special_groups where id_special_group = ?";
const constexpr char* SQL_UPDATE_SGROUP_CALLBACK = "update special_groups set callback_url = ? where id_special_group = ?";
const constexpr char* SQL_UPDATE_SGROUP_TOKEN = "update special_groups set sg_api_token = ? where id_special_group = ?";

const constexpr char* SQL_ADD_LINK_DESCRIPTOR_SGROUP =
  "insert ignore link_descriptor_sgroup set id_descriptor = ?, id_sgroup = ?";

const constexpr char* SQL_REMOVE_SG_FACE_DESCRIPTOR = R"_SQL_(
  delete from
    face_descriptors fd
  where
    fd.id_descriptor = ?
    and fd.id_descriptor in (select ldsg.id_descriptor from link_descriptor_sgroup ldsg where ldsg.id_sgroup = ?)
)_SQL_";

const constexpr char* SQL_REMOVE_SG_FACE_DESCRIPTORS = R"_SQL_(
  delete from
    face_descriptors
  where
    id_descriptor in (select ldsg.id_descriptor from link_descriptor_sgroup ldsg where ldsg.id_sgroup = ?)
)_SQL_";

const constexpr char* SQL_GET_SG_FACE_DESCRIPTORS = R"_SQL_(
  select
    di.id_descriptor,
    replace(concat('data:', di.mime_type, ';base64,', to_base64(di.face_image)), '\n', '') face_image
  from
    link_descriptor_sgroup ldsg
    inner join face_descriptors fd
      on ldsg.id_descriptor = fd.id_descriptor
    inner join descriptor_images di
      on fd.id_descriptor = di.id_descriptor
  where
    ldsg.id_sgroup = ?
 )_SQL_";

class Singleton
{
public:
  Singleton(Singleton const&) = delete;
  Singleton& operator= (Singleton const&) = delete;
  static Singleton& instance()
  {
    static Singleton s;
    return s;
  }

  std::unique_ptr<mysqlx::Client> sql_client;

  std::unique_ptr<concurrencpp::runtime> runtime;

  size_t descriptor_size = 512;
  size_t face_width = 112;
  size_t face_height = 112;

  std::atomic_bool is_working = true;

  std::shared_timed_mutex mtx_task_config;
  TaskConfig task_config;

  //люди и их данные
  HashMap<int, FaceDescriptor> id_descriptor_to_data;  //связка идентификатора дескриптора и его вектора
  HashMap<int, HashSet<int>> id_vstream_to_id_descriptors;  //связка идентификатора видео потока и дескрипторов
  HashMap<int, HashSet<int>> id_descriptor_to_id_vstreams;  //связка идентификатора дескриптора и видео потоков

  //специальные группы
  HashMap<int, HashSet<int>> id_sgroup_to_id_descriptors;  //связка идентификатора специальной группы и дескрипторов

  std::chrono::steady_clock::time_point tp_task_config;

  int id_worker = 0;  //идентификатор рабочего сервера FRS

  //конфигурационные параметры FRS
  std::filesystem::path logs_path = "./logs";
  std::filesystem::path screenshot_path = "./screenshots";
  std::filesystem::path events_path = "./events";
  String http_server_screenshot_url;
  String http_server_events_url;

  //прочие поля
  std::mutex mtx_append_event;
  std::mutex mtx_capture;
  HashSet<int> is_capturing;
  HashMap<int, time_point> id_vstream_to_start_motion;  //время последней детекции начала движения
  HashMap<int, time_point> id_vstream_to_end_motion;  //время последней детекции окончания движения
  HashSet<int> is_door_open;  //информация об открытой двери
  String working_path = "./";  //рабочая директория

  //для сбора статистики инференса
  std::mutex mtx_stats;
  HashMap<int, DNNStatsData> dnn_stats_data;

  //методы
  void addLog(const String& msg) const;
  void loadData();  //загрузка данных
  int addFaceDescriptor(int id_vstream, const FaceDescriptor& fd, const cv::Mat& f_img);  //добавление нового дескриптора лица
  std::tuple<int, String, String> addLogFace(int id_vstream, DateTime log_date, int id_descriptor, double quality,
    const cv::Rect& face_rect, const std::string& screenshot) const;
  void addLogDeliveryEvent(DeliveryEventType delivery_type, DeliveryEventResult delivery_result, int id_vstream, int id_descriptor) const;
  static double cosineDistance(const FaceDescriptor& fd1, const FaceDescriptor& fd2);
  void addPermanentTasks() const;

  template <typename T>
  T getConfigParamValue(absl::string_view param_name, int id_vstream = 0)
  {
    if (id_vstream != 0)
    {
      auto it_param = task_config.vstream_conf_params.find(id_vstream);
      if (it_param != task_config.vstream_conf_params.end())
      {
        auto it = it_param->second.find(param_name);
        if (it != it_param->second.end())
          return std::get<T>(it->second.value);
      }
    }

    auto it = task_config.conf_params.find(param_name);
    if (it != task_config.conf_params.end())
      return std::get<T>(it->second.value);

    return T{};
  };

  static String variantToString(const Variant& v);
  int addSGroupFaceDescriptor(int id_sgroup, const FaceDescriptor& fd, const cv::Mat& f_img);  //добавление нового дескриптора лица в специальную группу

  //для сбора статистики инференса
  void loadDNNStatsData();
  void saveDNNStatsData();

private:
  Singleton();
  virtual ~Singleton();
};
