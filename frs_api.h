/**
 * @api {post} /addStream Добавить видео поток
 * @apiName addStream
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String} [url] URL для захвата кадра с видео потока
 * @apiParam {String} [callback] URL для отправки событий о распознавании лиц (FRS --> callback)
 * @apiParam {Number[]} [faces] массив faceId (идентификаторов дескрипторов)
 * @apiParam {Object[]} [params] массив параметров настройки видео потока
 * @apiParam {String=
 *   "alpha",
 *   "best-quality-interval-after",
 *   "best-quality-interval-before",
 *   "beta",
 *   "blur",
 *   "blur-max",
 *   "capture-timeout",
 *   "comments-blurry-face",
 *   "comments-descriptor-creation-error",
 *   "comments-descriptor-exists",
 *   "comments-inference-error",
 *   "comments-new-descriptor",
 *   "comments-no-faces",
 *   "comments-non-frontal-face",
 *   "comments-non-normal-face-class",
 *   "comments-partial-face",
 *   "comments-url-image-error",
 *   "delay-between-frames",
 *   "descriptor-inactivity-period",
 *   "dnn-fc-inference-server",
 *   "dnn-fd-inference-server",
 *   "dnn-fr-inference-server",
 *   "face-class-confidence",
 *   "face-confidence",
 *   "face-enlarge-scale",
 *   "gamma",
 *   "logs-level",
 *   "margin",
 *   "max-capture-error-count",
 *   "pitch-threshold",
 *   "process-frames-interval",
 *   "region-x",
 *   "region-y",
 *   "region-width",
 *   "region-height",
 *   "retry-pause",
 *   "roll-threshold",
 *   "tolerance",
 *   "yaw-threshold"} params.paramName название параметра
 * @apiParam {Any} params.paramValue значение параметра, тип зависит от названия параметра
 * 
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234",
 *   "url": "http://host/getImage",
 *   "callback": "https://host/faceRecognized"
 *   "params": [
 *       {"paramName": "tolerance", "paramValue": 0.75},
 *       {"paramName": "face-enlarge-scale", "paramValue": 1.4}
 *   ]
 * }
 *
 */


/**
 * @api {post} /listStreams Получить список видео потоков и идентификаторов дескрипторов
 * @apiName listStreams
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiSuccess {Object[]} - массив объектов
 * @apiSuccess {String} -.streamId идентификатор видео потока
 * @apiSuccess {Number[]} [-.faces] массив faceId (идентификаторов дескрипторов)
 *
 * @apiSuccessExample {json} Пример результата выполнения:
 * [
 *   {
 *     "streamId": "1234",
 *     "faces": [123, 456, 789]
 *   },
 *   {
 *     "streamId": "1236",
 *     "faces": [456, 789, 910]
 *   }
 * ]
 */


/**
 * @api {post} /removeStream Убрать видео поток
 * @apiName removeStream
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * Метод отвязывает все дескрипторы и рабочий сервер от видео потока.
 * Если видео поток окажется без привязки к рабочему серверу, то он будет удален.
 *
 * @apiParam {String} streamId идентификатор видео потока
 *
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234"
 * }
 *
 */


/**
 * @api {post} /motionDetection Уведомление о детекции движения
 * @apiName motionDetection
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String="t","f"} start признак начала ("t") или конца ("f") детекции движения
 *
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234",
 *   "start": "t"
 * }
 *
 */


/**
 * @api {post} /bestQuality Получить "лучший" кадр из журнала FRS
 * @apiName bestQuality
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} [streamId] идентификатор видео потока (обязательный, если не указан eventId)
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} [date] дата события (обязательный, если указан streamId)
 * @apiParam {Number} [eventId] идентификатор события из журнала FRS (обязательный, если не указан streamId); если указан, то остальные параметры игнорируются
 * 
 * @apiParamExample {json} Пример использования:
 * {
 *   "eventId": 12345
 * }
 *
 * @apiSuccess {String} screenshot URL кадра
 * @apiSuccess {Number} left коордианата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} top коордианата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} width ширина прямоугольной области лица
 * @apiSuccess {Number} height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения:
 * {
 *   "screenshot": "https://...",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 */


/**
 * @api {post} /getEvents Получить список событий из журнала FRS
 * @apiName getEvents
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} dateStart начальная дата интервала событий
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} dateEnd конечная дата интервала событий
 *
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234",
 *   "dateStart": "2021-08-17 08:00:10",
 *   "dateEnd": "2021-08-17 09:00:10"
 * }
 *
 * @apiSuccess {Object[]} - массив объектов
 * @apiSuccess {String="yyyy-MM-dd hh:mm:ss.zzz"} -.date дата события
 * @apiSuccess {Number} [-.faceId] идентификатор зарегистрированного дескриптора распознанного лица
 * @apiSuccess {Number} -.quality параметр "качества" лица
 * @apiSuccess {String} -.screenshot URL кадра
 * @apiSuccess {Number} -.left коордианата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} -.top коордианата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} -.width ширина прямоугольной области лица
 * @apiSuccess {Number} -.height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения:
 * [
 *   {
 *     "date": "2021-08-17 08:00:15.834",
 *     "quality": 890,
 *     "screenshot": "https://...",
 *     "left": 537,
 *     "top": 438,
 *     "width": 142,
 *     "height": 156
 *   },
 *   {
 *     "date": "2021-08-17 08:30:15.071",
 *     "faceId": 1234,
 *     "quality": 1450,
 *     "screenshot": "https://...",
 *     "left": 637,
 *     "top": 338,
 *     "width": 175,
 *     "height": 182
 *   }
 * ]
 */


/**
 * @api {post} /registerFace Зарегистрировать дескриптор
 * @apiName registerFace
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String} url URL изображения для регистрации
 * @apiParam {Number} [left=0] коордианата X левого верхнего угла прямоугольной области лица
 * @apiParam {Number} [top=0] коордианата Y левого верхнего угла прямоугольной области лица
 * @apiParam {Number} [width="0 (вся ширина изображения)"] ширина прямоугольной области лица
 * @apiParam {Number} [height="0 (вся высота изображения)"] высота прямоугольной области лица
 *
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234",
 *   "url": "https://host/imageToRegister",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 *
 * @apiSuccess {Number} faceId идентификатор зарегистрированного дескриптора
 * @apiSuccess {String} faceImage URL изображения лица зарегистрированного дескриптора
 * @apiSuccess {Number} left коордианата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} top коордианата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} width ширина прямоугольной области лица
 * @apiSuccess {Number} height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения:
 * {
 *   "faceId": 4567,
 *   "faceImage": "data:image/jpeg,base64,...",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 */


/**
 * @api {post} /addFaces Добавить зарегистрированные дескрипторы к видео потоку
 * @apiName addFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 * 
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234",
 *   "faces": [123, 234, 4567]
 * }
 *
 */


/**
 * @api {post} /removeFaces Убрать зарегистрированные дескрипторы из видео потока
 * @apiName removeFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * Дескрипторы только отвязываются от видео потока, из базы не удаляются.
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 * 
 * @apiParamExample {json} Пример использования:
 * {
 *   "streamId": "1234",
 *   "faces": [123, 234, 4567]
 * }
 *
 */


/**
 * @api {post} /deleteFaces Удалить дескрипторы из базы
 * @apiName deleteFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * Дескрипторы безвозвратно удаляются из базы.
 *
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 *
 * @apiParamExample {json} Пример использования:
 * {
 *   "faces": [123, 234, 4567]
 * }
 *
 */


/**
 * @api {post} /listAllFaces Получить все идентификаторы дескрипторов
 * @apiName listAllFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * Возвращается полный список всех дескрипторов, которые есть в базе.
 *
 * @apiSuccess {Number[]} - массив faceId (идентификаторов дескрипторов)
 *
 * @apiSuccessExample {json} Пример результата выполнения:
 * [123, 456, 789]
 *
 */


/**
 * @api {post} callback Событие распознавания лица
 * @apiName faceRecognized
 * @apiGroup FRS-->Host
 * @apiVersion 1.0.0
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {Number} faceId идентификатор дескриптора распознанного лица
 * @apiParam {Number} eventId идентификатор события из журнала FRS
 *
 * @apiParamExample {json} Пример использования:
 * {
 *   "faceId": 4567,
 *   "eventId": 12345
 * }
 *
 */

#pragma once

//Wt
#include <Wt/WEnvironment.h>
#include <Wt/WResource.h>
#include <Wt/Http/Request.h>
#include <Wt/Http/Response.h>
#include <Wt/Http/ResponseContinuation.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Array.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>

//Qt
#include <QtCore>

#include "singleton.h"

namespace API
{
  //константы
  constexpr const int INDENT = 1;  //отступ в табуляциях для сериализации JSON
  constexpr const char* MIME_TYPE = "application/json";

  constexpr const char* MSG_DONE = "запрос выполнен";
  constexpr const char* MSG_UNAUTHORIZED = "не авторизован";
  constexpr const char* MSG_SERVER_ERROR = "ошибка на сервере";

  //код ответа
  constexpr const int CODE_SUCCESS = 200;
  constexpr const int CODE_NO_CONTENT = 204;
  constexpr const int CODE_ERROR = 400;
  constexpr const int CODE_UNAUTHORIZED = 401;
  constexpr const int CODE_FORBIDDEN = 403;
  constexpr const int CODE_NOT_ACCEPTABLE = 406;
  constexpr const int CODE_SERVER_ERROR = 500;

  //результат выполнения операции
  const QHash<int, const char*> RESPONSE_RESULT =
  {
      {CODE_SUCCESS, "OK"}
    , {CODE_NO_CONTENT, "No Content"}
    , {CODE_ERROR, "Bad Request"}
    , {CODE_UNAUTHORIZED, "Unauthorized"}
    , {CODE_NOT_ACCEPTABLE, "Not Acceptable"}
    , {CODE_SERVER_ERROR, "Internal Server Error"}
  };

  const QHash<int, const char*> RESPONSE_MESSAGE =
  {
      {CODE_SUCCESS, MSG_DONE}
    , {CODE_NO_CONTENT, "нет содержимого"}
    , {CODE_ERROR, "некорректный запрос"}
    , {CODE_UNAUTHORIZED, MSG_UNAUTHORIZED}
    , {CODE_NOT_ACCEPTABLE, "некорректное значение входных параметров в запросе"}
    , {CODE_SERVER_ERROR, MSG_SERVER_ERROR}
  };

  const QString DATETIME_FORMAT = "yyyy-MM-dd hh:mm:ss";
  const QString DATETIME_FORMAT_LOG_FACES = "yyyy-MM-dd hh:mm:ss.zzz";
  const QString INCORRECT_PARAMETER = QStringLiteral("Некорректное значение параметра \"%1\" в запросе.");
  const QString ERROR_SQL_PREPARE = QStringLiteral("Ошибка: не удалось подготовить запрос %1.");
  const QString ERROR_SQL_EXEC = QStringLiteral("Ошибка: не удалось выполнить запрос %1.");
  const QString ERROR_SQL_PREPARE_IN_FUNCTION = QStringLiteral("Ошибка: не удалось подготовить запрос %1 в функции %2.");
  const QString ERROR_SQL_EXEC_IN_FUNCTION = QStringLiteral("Ошибка: не удалось выполнить запрос %1 в функции %2.");

  //запросы
  const QString ADD_STREAM = QStringLiteral("/addstream");  // добавить или изменить видео поток
  const QString MOTION_DETECTION = QStringLiteral("/motiondetection");  //информация о детекции движения
  const QString BEST_QUALITY = QStringLiteral("/bestquality");  //получить информацию о кадре с "лучшим" лицом
  const QString REGISTER_FACE = QStringLiteral("/registerface");  //зарегистрировать лицо
  const QString ADD_FACES = QStringLiteral("/addfaces");  //привязать дескрипторы к видео потоку
  const QString REMOVE_FACES = QStringLiteral("/removefaces");  //отвязать дескрипторы от видео потока
  const QString LIST_STREAMS = QStringLiteral("/liststreams");  //список обрабатываемых видеопотоков и дескрипторов
  const QString REMOVE_STREAM = QStringLiteral("/removestream");  //убрать видеопоток
  const QString LIST_ALL_FACES = QStringLiteral("/listallfaces");  //список всех дескрипторов
  const QString DELETE_FACES = QStringLiteral("/deletefaces");  //удалить список дескрипторов из базы (в независимости от привязок к видеопотокам)
  const QString GET_EVENTS = QStringLiteral("/getevents");  //получить список событий из временного интервала

  //для теста
  const QString TEST_IMAGE = QStringLiteral("/testimage");  //протестировать изображение

  //для переноса дескрипторов используем виртуальный видеопоток, который не показываем в методе listStreams
  const QString VIRTUAL_VS = QStringLiteral("virtual");

  //параметры
  constexpr const char* P_CODE = "code";
  constexpr const char* P_NAME = "name";
  constexpr const char* P_MESSAGE = "message";

  constexpr const char* P_DATA = "data";
  constexpr const char* P_STREAM_ID = "streamId";
  constexpr const char* P_URL = "url";
  constexpr const char* P_FACE_IDS = "faces";
  constexpr const char* P_CALLBACK_URL = "callback";
  constexpr const char* P_START = "start";
  constexpr const char* P_DATE = "date";
  constexpr const char* P_LOG_FACES_ID = "eventId";
  constexpr const char* P_SCREENSHOT = "screenshot";
  constexpr const char* P_FACE_LEFT = "left";
  constexpr const char* P_FACE_TOP = "top";
  constexpr const char* P_FACE_WIDTH = "width";
  constexpr const char* P_FACE_HEIGHT = "height";
  constexpr const char* P_FACE_ID = "faceId";
  constexpr const char* P_FACE_IMAGE = "faceImage";
  constexpr const char* P_PARAMS = "params";
  constexpr const char* P_PARAM_NAME = "paramName";
  constexpr const char* P_PARAM_VALUE = "paramValue";
  constexpr const char* P_QUALITY = "quality";
  constexpr const char* P_DATE_START = "dateStart";
  constexpr const char* P_DATE_END = "dateEnd";
}

struct ApiResponseStatus
{
  int code{};
  QString msg = "";
};

class ApiResource : public Wt::WResource
{
public:
  void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response) override;
  ~ApiResource() override;

private:
  static bool checkInputParam(const Wt::Json::Object& json, Wt::Http::Response& response, const char* input_param);
  static void simpleResponse(int code, Wt::Http::Response& response);
  static void simpleResponse(int code, const QString& msg, Wt::Http::Response& response);

  //api функции
  static bool addVStream(const QString& vstream_ext, const QString& url, const QString& callback_url, const std::vector<int>& face_ids,
    const QHash<QString, QString>& params);
  static void motionDetection(const QString& vstream_ext, bool is_start);
  static bool addFaces(const QString& vstream_ext, const std::vector<int>& face_ids);
  static bool removeFaces(const QString& vstream_ext, const std::vector<int>& face_ids);
  static bool deleteFaces(const std::vector<int>& face_ids);
  static bool removeVStream(const QString& vstream_ext);
};
