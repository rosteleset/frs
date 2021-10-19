#pragma once

#include <chrono>
#include <QtCore>
#include <QtSql>

#include <opencv2/opencv.hpp>
#include "sql_pool.h"
#include "scheduler.h"

struct HeadPoseEstimation
{
  float angle_r = 0.0f;
  float angle_p = 0.0f;
  float angle_y = 0.0f;
  vector<cv::Point2f> axis;
};

typedef cv::Mat FaceDescriptor;

struct ConfParam
{
  QVariant value;  // значение параметра
  QString comments;  // описание параметра
  QVariant min_value;  // минимальное значение (если есть)
  QVariant max_value;  // максимальное значение (если есть)
};

enum LogsLevel
{
  LOGS_ERROR = 0,
  LOGS_EVENTS,
  LOGS_VERBOSE
};

//типы регистрации человека
enum RegistrationType
{
  REGISTERED = 0,  // зарегистрированный
  UNKNOWN  // неизвестный
};

struct TaskConfig
{
  //конфигурационные параметры (начало)
  QHash<QString, ConfParam> conf_params;  //общие конфигурационные параметры системы

  //явно указанные параметры видеопотоков
  QHash<QString, int> conf_vstream_ext_to_id_vstream;  //связка внешнего идентификатора видеопотока с внутренним
  QHash<int, QString> conf_id_vstream_to_vstream_ext;  //связка внутреннего идентификатора видеопотока с внешним
  QHash<int, QString> conf_vstream_url;  //URL скриншотов с камер
  QHash<int, QString> conf_vstream_callback_url;  //URL для вызова методов
  QHash<int, cv::Rect> conf_vstream_region;  //связка камеры и области, в которой определяются лица
  QHash<int, QHash<QString, ConfParam>> vstream_conf_params;  //индивидуальные параметры для камер

  //конфигурационные параметры (конец)

  TaskConfig()
  {
    conf_params.setSharable(false);
    conf_vstream_ext_to_id_vstream.setSharable(false);
    conf_id_vstream_to_vstream_ext.setSharable(false);
    conf_vstream_url.setSharable(false);
    conf_vstream_callback_url.setSharable(false);
    conf_vstream_region.setSharable(false);
    vstream_conf_params.setSharable(false);
  }

  /*~TaskConfig()
  {
    std::cout << "TaskConig destroyed\n";
  }*/
};

const QString DATETIME_FORMAT = "dd.MM.yyyy HH:mm:ss";

//Константы с названием параметров конфигурации
const QString CONF_FILE = "file";  // = ".config";
const QString CONF_LOGS_LEVEL = "logs-level";  // = 2;  //уровень логов: 0 - ошибки; 1 - события; 2 - подробно
const QString CONF_FACE_CONFIDENCE_THRESHOLD = "face-confidence";  // = 0.8;  //порог вероятности определения лица
const QString CONF_FACE_CLASS_CONFIDENCE_THRESHOLD = "face-class-confidence";  // = 0.7;  //порог вероятности определения макси или темных очков на лице
const QString CONF_LAPLACIAN_THRESHOLD = "blur";  // = 50.0;  //нижний порог для проверки на размытость изображения (через дискретный аналог оператора Лапласа)
const QString CONF_LAPLACIAN_THRESHOLD_MAX = "blur-max";  // = 15000.0;  //верхний порог для проверки на размытость изображения (через дискретный аналог оператора Лапласа)
const QString CONF_TOLERANCE = "tolerance";  // = 0.7;  //толерантность - параметр, определяющий, что два вектора-дескриптора лица принадлежат одному человеку
const QString CONF_MAX_CAPTURE_ERROR_COUNT = "max-capture-error-count"; // = 3;  // Максимальное количество подряд полученных ошибок при получении кадра с камеры, после которого будет считаться попытка получения кадра неудачной
const QString CONF_RETRY_PAUSE = "retry-pause";  // = 30;  //пазуа в секундах перед следующей попыткой установки связи с видеопотоком
const QString CONF_FACE_ENLARGE_SCALE = "face-enlarge-scale";  // = 1.4; //коэффициент расширения прямоугольной области лица для хранения
const QString CONF_ALPHA = "alpha";  // = 1.0;  //альфа-коррекция
const QString CONF_BETA = "beta";  // = 0;  //бета-коррекция
const QString CONF_GAMMA = "gamma";  // = 1.0;  //гамма-коррекция
const QString CONF_CAPTURE_TIMEOUT = "capture-timeout"; // = 1.0;  //время ожидания захвата кадра с камеры в секундах
const QString CONF_MARGIN = "margin";  // = 5.0;  //отступ в процентах от краев кадра для уменьшения рабочей области
const QString CONF_DESCRIPTOR_INACTIVITY_PERIOD = "descriptor-inactivity-period";  // = 400;  //период неактивности дескриптора в днях, спустя который он удаляется
const QString CONF_BEST_QUALITY_INTERVAL_BEFORE = "best-quality-interval-before";  // = 5.0;  //период в секундах до временной точки для поиска лучшего кадра
const QString CONF_BEST_QUALITY_INTERVAL_AFTER = "best-quality-interval-after";  // = 2.0;  //период в секундах после временной точки для поиска лучшего кадра

//для детекции движения
const QString CONF_PROCESS_FRAMES_INTERVAL = "process-frames-interval";  // = 1.0  // интервал в секундах, втечение которого обрабатываются кадры после окончания детекции движения
const QString CONF_DELAY_BETWEEN_FRAMES = "delay-between-frames";  // = 1.0  // интервал в секундах между захватами кадров после детекции движения

//пороговые значения параметров roll, yaw, pitch, которые задают интервал в градусах, для определения фронтальности лица
const QString CONF_ROLL_THRESHOLD = "roll-threshold";  // = 45.0;  //интервал (-conf-roll_threshold; +conf_roll_threshold)
const QString CONF_YAW_THRESHOLD = "yaw-threshold";  // = 15.0;  //интервал (-conf-yaw_threshold; +conf_yaw_threshold)
const QString CONF_PITCH_THRESHOLD = "pitch-threshold";  // = 30.0;  //интервал (-conf-pitch_threshold; +conf_pitch_threshold)

//для периодического удаления устаревших логов из таблицы log_faces
const QString CONF_CLEAR_OLD_LOG_FACES = "clear-old-log-faces";  // = 8.0;  //период запуска очистки устаревших логов из таблицы log_faces
const QString CONF_LOG_FACES_LIVE_INTERVAL = "log-faces-live-interval";  // = 24.0;  //"время жизни" логов из таблицы log_faces

//для комментариев API методов
const QString CONF_COMMENTS_NO_FACES = "comments-no-faces";
const QString CONF_COMMENTS_PARTIAL_FACE = "comments-partial-face";
const QString CONF_COMMENTS_NON_FRONTAL_FACE = "comments-non-frontal-face";
const QString CONF_COMMENTS_NON_NORMAL_FACE_CLASS = "comments-non-normal-face-class";
const QString CONF_COMMENTS_BLURRY_FACE = "comments-blurry-face";
const QString CONF_COMMENTS_NEW_DESCRIPTOR = "comments-new-descriptor";
const QString CONF_COMMENTS_DESCRIPTOR_EXISTS = "comments-descriptor-exists";
const QString CONF_COMMENTS_DESCRIPTOR_CREATION_ERROR = "comments-descriptor-creation-error";
const QString CONF_COMMENTS_INFERENCE_ERROR = "comments-inference-error";
const QString CONF_COMMENTS_URL_IMAGE_ERROR = "comments-url-image-error";

//для инференса детекции лиц
const QString CONF_DNN_FD_INFERENCE_SERVER = "dnn-fd-inference-server";  //сервер
const QString CONF_DNN_FD_MODEL_NAME = "dnn-fd-model-name";  //название модели нейронной сети
const QString CONF_DNN_FD_INPUT_TENSOR_NAME = "dnn-fd-input-tensor-name";  //название входного тензора
const QString CONF_DNN_FD_INPUT_WIDTH = "dnn-fd-input-width";  //ширина изображения
const QString CONF_DNN_FD_INPUT_HEIGHT = "dnn-fd-input-height";  //высота изображения

//для инференса получения класса лица (нормальное, в маске, без маски)
const QString CONF_DNN_FC_INFERENCE_SERVER = "dnn-fc-inference-server";  //сервер
const QString CONF_DNN_FC_MODEL_NAME = "dnn-fc-model-name";  //название модели нейронной сети
const QString CONF_DNN_FC_INPUT_TENSOR_NAME = "dnn-fc-input-tensor-name";  //название входного тензора
const QString CONF_DNN_FC_INPUT_WIDTH = "dnn-fc-input-width";  //ширина изображения
const QString CONF_DNN_FC_INPUT_HEIGHT = "dnn-fc-input-height";  //высота изображения
const QString CONF_DNN_FC_OUTPUT_TENSOR_NAME = "dnn-fc-output-tensor-name";  //название выходного тензора
const QString CONF_DNN_FC_OUTPUT_SIZE = "dnn-fc-output-size";  //размер массива результата инференса

//для инференса получения дескриптора лица
const QString CONF_DNN_FR_INFERENCE_SERVER = "dnn-fr-inference-server";  //сервер
const QString CONF_DNN_FR_MODEL_NAME = "dnn-fr-model-name";  //название модели нейронной сети
const QString CONF_DNN_FR_INPUT_TENSOR_NAME = "dnn-fr-input-tensor-name";  //название входного тензора
const QString CONF_DNN_FR_INPUT_WIDTH = "dnn-fr-input-width";  //ширина изображения
const QString CONF_DNN_FR_INPUT_HEIGHT = "dnn-fr-input-height";  //высота изображения
const QString CONF_DNN_FR_OUTPUT_TENSOR_NAME = "dnn-fr-output-tensor-name";  //название выходного тензора
const QString CONF_DNN_FR_OUTPUT_SIZE = "dnn-fr-output-size";  //размер массива результата инференса

//SQL запросы
const QString SQL_GET_CURRENT_TIME_POINT = "select unix_timestamp(current_timestamp) current_time_point";
const QString SQL_GET_COMMON_SETTINGS = "select *, unix_timestamp(time_point) unix_time_point from common_settings";

const QString SQL_UPDATE_COMMON_SETTINGS = R"_SQL_(
  update
    common_settings cs
  set
    cs.param_value = :param_value,
    cs.time_point = from_unixtime(:time_point)
  where
    cs.param_name = :param_name
)_SQL_";

const QString SQL_GET_VIDEO_STREAMS = R"_SQL_(
  select
    vs.*
  from
    link_worker_vstream lw
    inner join video_streams vs
      on vs.id_vstream = lw.id_vstream
      and lw.id_worker = :id_worker
)_SQL_";

const QString SQL_GET_VIDEO_STREAM_BY_EXT = "select * from video_streams where vstream_ext = :vstream_ext";

const QString SQL_GET_VIDEO_STREAM_SETTINGS = R"_SQL_(
  select
    vss.*
  from
    link_worker_vstream lw
    inner join video_stream_settings vss
      on vss.id_vstream = lw.id_vstream
      and lw.id_worker = :id_worker
)_SQL_";

const QString SQL_GET_VIDEO_STREAM_PARAM = R"_SQL_(
  select
    *
  from
    video_stream_settings
  where
    id_vstream = :id_vstream
    and param_name = :param_name
)_SQL_";

const QString SQL_SET_VIDEO_STREAM_PARAM = R"_SQL_(
  insert into
    video_stream_settings
  set
    id_vstream = :id_vstream,
    param_name = :param_name,
    param_value = :param_value
  on duplicate key update
    param_value = :param_value
)_SQL_";

const QString SQL_REMOVE_VIDEO_STREAM_PARAM = R"_SQL_(
  delete from
    video_stream_settings
  where
    id_vstream = :id_vstream
    and param_name = :param_name
)_SQL_";

const QString SQL_ADD_VSTREAM = R"_SQL_(
  insert into
    video_streams
  set
    vstream_ext = :vstream_ext,
    url = :url,
    callback_url = :callback_url,
    region_x = :region_x,
    region_y = :region_y,
    region_width = :region_width,
    region_height = :region_height
)_SQL_";

const QString SQL_UPDATE_VSTREAM = R"_SQL_(
  update
    video_streams vs
  set
    url = :url,
    callback_url = :callback_url,
    vs.region_x = :region_x,
    vs.region_y = :region_y,
    vs.region_width = :region_width,
    vs.region_height = :region_height
  where
    vs.id_vstream = :id_vstream
 )_SQL_";

const QString SQL_REMOVE_VSTREAM = R"_SQL_(
  delete from
    video_streams vs
  where
    vs.id_vstream = :id_vstream
)_SQL_";

const QString SQL_GET_FACE_DESCRIPTORS = R"_SQL_(
  select distinct
    fd.*
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = :id_worker
    inner join face_descriptors fd
      on fd.id_descriptor = ld.id_descriptor
)_SQL_";

const QString SQL_GET_FACE_DESCRIPTOR_BY_ID = "select * from face_descriptors where id_descriptor = :id_descriptor";
const QString SQL_GET_ALL_FACE_DESCRIPTORS = "select id_descriptor from face_descriptors";

const QString SQL_ADD_FACE_DESCRIPTOR = R"_SQL_(
  insert into
    face_descriptors
  set
    descriptor_data = :descriptor_data
  )_SQL_";

const QString SQL_UPDATE_FACE_DESCRIPTOR = R"_SQL_(
  update
    face_descriptors
  set
    date_last = current_stamp
  where
    id_descriptor = :id_descriptor
)_SQL_";

const QString SQL_REMOVE_FACE_DESCRIPTOR = "delete from face_descriptors fd where fd.id_descriptor = :id_descriptor";

const QString SQL_ADD_DESCRIPTOR_IMAGE = R"_SQL_(
  insert into
    descriptor_images
  set
    id_descriptor = :id_descriptor,
    mime_type = :mime_type,
    face_image = :face_image
)_SQL_";

const QString SQL_GET_LINKS_DESCRIPTOR_VSTREAM = R"_SQL_(
  select
    ld.*
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = :id_worker
)_SQL_";

const QString SQL_GET_LINKS_DESCRIPTOR_VSTREAM_BY_ID = R"_SQL_(
  select
    ld.id_descriptor
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = :id_worker
      and ld.id_vstream = :id_vstream
)_SQL_";

const QString SQL_CHECK_LINKS_DESCRIPTOR_VSTREAM = R"_SQL_(
  select
    ld.id_descriptor
  from
    link_worker_vstream lw
    inner join link_descriptor_vstream ld
      on ld.id_vstream = lw.id_vstream
      and lw.id_worker = :id_worker
      and ld.id_descriptor = :id_descriptor
  limit 1
)_SQL_";

const QString SQL_ADD_LINK_DESCRIPTOR_VSTREAM =
  "insert ignore link_descriptor_vstream set id_descriptor = :id_descriptor, id_vstream = :id_vstream";

const QString SQL_REMOVE_LINK_DESCRIPTOR_VSTREAM =
  "delete from link_descriptor_vstream where id_descriptor = :id_descriptor and id_vstream = :id_vstream";

//получение дескрипторов, которые привязаны только к одному указанному видео потоку
const QString SQL_GET_SOLE_LINK_DESCRIPTORS_VSTREAM = R"_SQL_(
  select
    ldv.id_descriptor
  from
    link_descriptor_vstream ldv
  where
    ldv.id_vstream = :id_vstream
    and not exists(select * from link_descriptor_vstream where id_vstream != :id_vstream and id_descriptor = ldv.id_descriptor)
)_SQL_";

const QString SQL_ADD_LOG_FACE = R"_SQL_(
  insert into
    log_faces
  set
    id_vstream = :id_vstream,
    log_date = :log_date,
    id_descriptor = :id_descriptor,
    quality = :quality,
    screenshot = :screenshot,
    face_left = :face_left,
    face_top = :face_top,
    face_width = :face_width,
    face_height = :face_height
)_SQL_";

const QString SQL_GET_LOG_FACE_BEST_QUALITY = R"_SQL_(
  select
    l.screenshot,
    l.face_left,
    l.face_top,
    l.face_width,
    l.face_height
  from
    log_faces l
  where
    l.id_vstream = :id_vstream
    and (l.log_date >= :log_date - interval :interval_before second) and (l.log_date <= :log_date + interval :interval_after second)
  order by
    l.quality desc
  limit
    1
)_SQL_";

const QString SQL_GET_LOG_FACE_BY_ID = R"_SQL_(
  select
    l.screenshot,
    l.face_left,
    l.face_top,
    l.face_width,
    l.face_height
  from
    log_faces l
  where
    l.id_log = :id_log
)_SQL_";

const QString SQL_GET_LOG_FACES_FROM_INTERVAL = R"_SQL_(
  select
    l.log_date,
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
    l.id_vstream = :id_vstream
    and l.log_date >= :date_start
    and l.log_date <= :date_end
  order by
    l.log_date
)_SQL_";

const QString SQL_REMOVE_OLD_LOG_FACES = "delete from log_faces where log_date < :log_date";

const QString SQL_ADD_LOG_DELIVERY_EVENT = R"_SQL_(
  insert ignore into
    log_delivery_events
  set
    delivery_type = :delivery_type,
    delivery_result = :delivery_result,
    id_vstream = :id_vstream,
    id_descriptor = :id_descriptor
)_SQL_";

const QString SQL_GET_WORKER_BY_ID = "select id_worker from workers where id_worker = %1";

const QString SQL_ADD_LINK_WORKER_VSTREAM = R"_SQL_(
  insert into
    link_worker_vstream
  set
    id_worker = :id_worker,
    id_vstream = :id_vstream
  on duplicate key update
    id_worker = :id_worker
)_SQL_";

const QString SQL_REMOVE_LINK_WORKER_VSTREAM = R"_SQL_(
  delete from
    link_worker_vstream
  where
    id_worker = :id_worker
    and id_vstream = :id_vstream
)_SQL_";

class SqlTransaction
{
public:
  explicit SqlTransaction(QString  connection_name, bool do_start = true);
  virtual ~SqlTransaction();
  bool start();
  bool commit();
  bool rollback();
  bool inTransaction() const;
  QSqlDatabase getDatabase();

private:
  QString conn_name = "";
  bool in_transaction = false;
};

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

  unique_ptr<Scheduler> task_scheduler;
  unique_ptr<SQLPool> sql_pool;

  size_t descriptor_size = 512;
  size_t face_width = 112;
  size_t face_height = 112;

  std::atomic_bool is_working = true;

  std::mutex mtx_task_config;
  TaskConfig task_config;

  //люди и их данные
  QHash<int, FaceDescriptor> id_descriptor_to_data;  //свзяка идентификатора дескриптора и его вектора
  QHash<int, QSet<int>> id_vstream_to_id_descriptors;  //связка идентификатора видеопотока и дескрипторов
  QHash<int, QSet<int>> id_descriptor_to_id_vstreams;  //связка идентификатора дескриптора и видеопотоков

  std::chrono::steady_clock::time_point tp_task_config;

  int id_worker = 0;  //идентификатор рабочего сервера FRS

  //конфигурационные параметры FRS
  QString logs_path = "./logs";
  QString screenshot_path = "./screenshots";
  QString http_server_screenthot_url = "";

  //прочие поля
  std::mutex mtx_capture;
  QSet<int> is_capturing;
  QHash<int, time_point> id_vstream_to_start_motion;  //время последней детекции начала движения
  QHash<int, time_point> id_vstream_to_end_motion;  //время последней детекции окончания движения
  QString working_path = "./";  //рабочая директория

  //методы
  void addLog(const QString& msg) const;
  void loadData();  //загрузка данных
  int addFaceDescriptor(int id_vstream, const FaceDescriptor& fd, const cv::Mat& f_img);  //добавление нового дескриптора лица
  int addLogFace(int id_vstream, const QDateTime& log_date, int id_descriptor, double quality, const cv::Rect& face_rect, const std::string& screenshot) const;
  void removeOldLogFaces();
  void addLogDeliveryEvent(DeliveryEventType delivery_type, DeliveryEventResult delivery_result, int id_vstream, int id_descriptor) const;

  static double cosineDistance(const FaceDescriptor& fd1, const FaceDescriptor& fd2);
private:
  Singleton();
  ~Singleton();
};

class SQLGuard
{
public:
  ~SQLGuard()
  {
    if (Singleton::instance().sql_pool != nullptr)
      Singleton::instance().sql_pool->releaseConnection(conn_name);
  }

  QString connectionName()
  {
    if (conn_name.isEmpty())
    {
      if (Singleton::instance().sql_pool != nullptr)
        conn_name = Singleton::instance().sql_pool->acquireConnection();
      else
        conn_name = "";
    }
    return conn_name;
  }

private:
  QString conn_name = "";
};
