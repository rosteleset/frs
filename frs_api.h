/**
 * @api {post} /api/addStream Добавить видео поток
 * @apiPrivate
 * @apiName addStream
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
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
 *   "callback-timeout",
 *   "capture-timeout",
 *   "clear-old-log-faces",
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
 *   "dnn-fc-input-height",
 *   "dnn-fc-input-tensor-name",
 *   "dnn-fc-input-width",
 *   "dnn-fc-model-name",
 *   "dnn-fc-output-size",
 *   "dnn-fc-output-tensor-name",
 *   "dnn-fd-inference-server",
 *   "dnn-fd-input-height",
 *   "dnn-fd-input-tensor-name",
 *   "dnn-fd-input-width",
 *   "dnn-fd-model-name",
 *   "dnn-fr-inference-server",
 *   "dnn-fr-input-height",
 *   "dnn-fr-input-tensor-name",
 *   "dnn-fr-input-width",
 *   "dnn-fr-model-name",
 *   "dnn-fr-output-size",
 *   "dnn-fr-output-tensor-name",
 *   "face-class-confidence",
 *   "face-confidence",
 *   "face-enlarge-scale",
 *   "flag-copy-event-data",
 *   "gamma",
 *   "log-faces-live-interval",
 *   "logs-level",
 *   "margin",
 *   "max-capture-error-count",
 *   "open-door-duration",
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
 * @apiParamExample {json} Пример использования
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
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/listStreams Получить список видео потоков и идентификаторов дескрипторов
 * @apiPrivate
 * @apiName listStreams
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiSuccess {Object[]} - массив объектов
 * @apiSuccess {String} -.streamId идентификатор видео потока
 * @apiSuccess {Number[]} [-.faces] массив faceId (идентификаторов дескрипторов)
 *
 * @apiSuccessExample {json} Пример результата выполнения
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
 *
 * @apiErrorExample {json} Ошибки
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/removeStream Убрать видео поток
 * @apiPrivate
 * @apiName removeStream
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Метод отвязывает все дескрипторы и рабочий сервер от видео потока.
 * Если видео поток окажется без привязки к рабочему серверу, то он будет удален.
 *
 * @apiParam {String} streamId идентификатор видео потока
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "streamId": "1234"
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/motionDetection Уведомление о детекции движения
 * @apiPrivate
 * @apiName motionDetection
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String} start признак начала ("t") или конца (любое значение, не равное "t", например, "f") детекции движения
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "streamId": "1234",
 *   "start": "t"
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 */


/**
 * @api {post} /api/doorIsOpen Уведомление об открытии двери
 * @apiPrivate
 * @apiName doorIsOpen
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "streamId": "1234"
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 */


/**
 * @api {post} /api/bestQuality Получить "лучший" кадр из журнала FRS
 * @apiPrivate
 * @apiName bestQuality
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} [streamId] идентификатор видео потока (обязательный, если не указан eventId)
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} [date] дата события (обязательный, если указан streamId)
 * @apiParam {Number} [eventId] идентификатор события из журнала FRS (обязательный, если не указан streamId); если указан, то остальные параметры игнорируются
 * @apiParam {String} [uuid] uuid события хоста
 * 
 * @apiParamExample {json} Пример использования
 * {
 *   "eventId": 12345
 * }
 *
 * @apiSuccess {String} screenshot URL кадра
 * @apiSuccess {Number} left координата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} top координата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} width ширина прямоугольной области лица
 * @apiSuccess {Number} height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * {
 *   "screenshot": "https://...",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/getEvents Получить список событий из журнала FRS
 * @apiPrivate
 * @apiName getEvents
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} dateStart начальная дата интервала событий
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} dateEnd конечная дата интервала событий
 *
 * @apiParamExample {json} Пример использования
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
 * @apiSuccess {Number} -.left координата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} -.top координата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} -.width ширина прямоугольной области лица
 * @apiSuccess {Number} -.height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения
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
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/registerFace Зарегистрировать дескриптор
 * @apiPrivate
 * @apiName registerFace
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {String} url URL изображения для регистрации
 * @apiParam {Number} [left=0] координата X левого верхнего угла прямоугольной области лица
 * @apiParam {Number} [top=0] координата Y левого верхнего угла прямоугольной области лица
 * @apiParam {Number} [width="0 (вся ширина изображения)"] ширина прямоугольной области лица
 * @apiParam {Number} [height="0 (вся высота изображения)"] высота прямоугольной области лица
 *
 * @apiParamExample {json} Пример использования
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
 * @apiSuccess {Number} left координата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} top координата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} width ширина прямоугольной области лица
 * @apiSuccess {Number} height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * {
 *   "faceId": 4567,
 *   "faceImage": "data:image/jpeg,base64,...",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/addFaces Добавить зарегистрированные дескрипторы к видео потоку
 * @apiPrivate
 * @apiName addFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 * 
 * @apiParamExample {json} Пример использования
 * {
 *   "streamId": "1234",
 *   "faces": [123, 234, 4567]
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/removeFaces Убрать зарегистрированные дескрипторы из видео потока
 * @apiPrivate
 * @apiName removeFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Дескрипторы только отвязываются от видео потока, из базы не удаляются.
 *
 * @apiParam {String} streamId идентификатор видео потока
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 * 
 * @apiParamExample {json} Пример использования
 * {
 *   "streamId": "1234",
 *   "faces": [123, 234, 4567]
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/deleteFaces Удалить дескрипторы из базы
 * @apiPrivate
 * @apiName deleteFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Дескрипторы безвозвратно удаляются из базы.
 *
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "faces": [123, 234, 4567]
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 *
 */


/**
 * @api {post} /api/listAllFaces Получить все идентификаторы дескрипторов
 * @apiPrivate
 * @apiName listAllFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Возвращается полный список всех дескрипторов, которые есть в базе.
 *
 * @apiSuccess {Number[]} - массив faceId (идентификаторов дескрипторов)
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * [123, 456, 789]
 *
 * @apiErrorExample {json} Ошибки
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/getSettings Получить параметры настройки
 * @apiPrivate
 * @apiName getSettings
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String[]} [params] массив с названием интересующих параметров настройки; если не указан, то возвращаются все параметры
 *
 * @apiSuccess {Object[]} - массив объектов
 * @apiSuccess {String} -.paramName название параметра
 * @apiSuccess {Any} -.paramValue значение параметра
 * @apiSuccess {String="bool", "int", "double", "string"} -.paramType тип параметра
 * @apiSuccess {String} [-.description] описание параметра
 * @apiSuccess {Any} [-.minValue] минимальное значение параметра
 * @apiSuccess {Any} [-.maxValue] максимальное значение параметра
 *
 * @apiParamExample {json} Пример использования
 * {
 *	 "params": ["flag-copy-event-data", "retry-pause", "face-confidence", "comments-non-normal-face-class"]
 * }
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * [
 *   {
 *     "paramName": "flag-copy-event-data",
 *     "paramValue": true,
 *     "paramType": "bool"
 *   },
 *   {
 *     "paramName": "retry-pause",
 *     "paramValue": 30,
 *     "paramType": "int",
 *     "description": "Пауза в секундах между попытками установки связи с видео потоком"
 *     "minValue": 3,
 *     "maxValue": 36000,
 *   },
 *   {
 *     "paramName": "face-confidence",
 *     "paramValue": 0.75,
 *     "paramType": "double",
 *     "description": "Порог вероятности определения лица",
 *     "minValue": 0.1,
 *     "maxValue": 1.0
 *   },
 *   {
 *     "paramName": "comments-non-normal-face-class",
 *     "paramValue": "Лицо в маске или в темных очках.",
 *     "paramType": "string"
 *   }
 * ]
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 */


/**
 * @api {post} /api/setSettings Установить параметры настройки
 * @apiPrivate
 * @apiName setSettings
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {Object[]} params массив параметров настройки
 * @apiParam {String} params.paramName название параметра
 * @apiParam {Any} params.paramValue значение параметра, тип зависит от названия параметра
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "params": [
 *       {"paramName": "tolerance", "paramValue": 0.75},
 *       {"paramName": "face-enlarge-scale", "paramValue": 1.4}
 *   ]
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/addSpecialGroup Добавить специальную группу
 * @apiPrivate
 * @apiName addSpecialGroup
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {String} groupName уникальное название специальной группы
 * @apiParam {Number{> 0}} [maxDescriptorCount=1000] максимальное количество дескрипторов в специальной группе
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "groupName": "Моя специальная группа",
 *   "maxDescriptorCount": 5000
 * }
 *
 * @apiSuccess {Number} groupId идентификатор специальной группы
 * @apiSuccess {String} accessApiToken токен авторизации для вызова API методов
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * {
 *   "groupId": 2,
 *   "accessApiToken": "7f66b071-45cf-4fe5-bd2f-503d132d8746"
 * }
 *
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/updateSpecialGroup Обновить специальную группу
 * @apiPrivate
 * @apiName updateSpecialGroup
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {Number} groupId идентификатор специальной группы
 * @apiParam {String} [groupName] новое уникальное название специальной группы
 * @apiParam {Number{> 0}} [maxDescriptorCount] новое максимальное количество дескрипторов в специальной группе
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "groupId": 2,
 *   "maxDescriptorCount": 1500
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/deleteSpecialGroup Удалить специальную группу
 * @apiPrivate
 * @apiName deleteSpecialGroup
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Все данные специальной группы, включая параметры и дескрипторы, безвозвратно удаляются из базы.
 *
 * @apiParam {Number} groupId идентификатор специальной группы
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "groupId": 2
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/processFrame Обработать кадр по URL
 * @apiPrivate
 * @apiName processFrame
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Обрабатывается кадр по указанному URL.
 *
 * @apiParam {String} [streamId] идентификатор видео потока (обязательный, если не указан groupId)
 * @apiParam {Number} [groupId] идентификатор специальной группы (обязательный, если не указан streamId)
 * @apiParam {String} url URL изображения для обработки
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "groupId": 2,
 *   "url": "data:image/jpeg,base64,..."
 * }
 *
 * @apiSuccess {Number[]} [-] массив faceId (идентификаторов дескрипторов), которые обнаружены в кадре
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 500 ошибка на сервере
 */


/**
 * @api {post} /api/saveDnnStatsData Сохранить данные сбора статистики инференса
 * @apiPrivate
 * @apiName saveDnnStatsData
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 */


/**
 * @apiDefine Authorization
 * @apiHeader {String} Authorization токен авторизации
 * @apiHeaderExample Пример
 * Bearer 7f66b071-45cf-4fe5-bd2f-503d132d8746
 */


/**
 * @api {post} /sgapi/sgRegisterFace Зарегистрировать дескриптор в специальной группе
 * @apiName sgRegisterFace
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiUse Authorization
 *
 * @apiParam {String} url URL изображения для регистрации
 * @apiParam {Number} [left=0] координата X левого верхнего угла прямоугольной области лица
 * @apiParam {Number} [top=0] координата Y левого верхнего угла прямоугольной области лица
 * @apiParam {Number} [width="0 (вся ширина изображения)"] ширина прямоугольной области лица
 * @apiParam {Number} [height="0 (вся высота изображения)"] высота прямоугольной области лица
 *
 * @apiParamExample {json} Пример 1
 * {
 *   "url": "https://host/imageToRegister",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 *
 * @apiParamExample {json} Пример 2
 * {
 *   "url": "data:image/jpeg,base64,..."
 * }
 *
 * @apiSuccess {Number} faceId идентификатор зарегистрированного дескриптора
 * @apiSuccess {String} faceImage URL изображения лица зарегистрированного дескриптора
 * @apiSuccess {Number} left координата X левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} top координата Y левого верхнего угла прямоугольной области лица
 * @apiSuccess {Number} width ширина прямоугольной области лица
 * @apiSuccess {Number} height высота прямоугольной области лица
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * {
 *   "faceId": 4567,
 *   "faceImage": "data:image/jpeg,base64,...",
 *   "left": 537,
 *   "top": 438,
 *   "width": 142,
 *   "height": 156
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 401 не авторизован
 * 403 запрещено
 * 500 ошибка на сервере
 */


/**
 * @api {post} /sgapi/sgDeleteFaces Удалить дескрипторы из специальной группы
 * @apiName sgDeleteFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * Дескрипторы безвозвратно удаляются из базы.
 *
 * @apiUse Authorization
 *
 * @apiParam {Number[]} faces массив faceId (идентификаторов дескрипторов)
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "faces": [123, 234, 4567]
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 401 не авторизован
 * 500 ошибка на сервере
 */


/**
 * @api {post} /sgapi/sgListFaces Получить список дескрипторов специальной группы
 * @apiName sgListFaces
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiUse Authorization
 *
 * @apiSuccess {Object[]} [-] массив объектов
 * @apiSuccess {Number} -.faceId идентификатор дескриптора
 * @apiSuccess {String} -.faceImage URL изображения лица
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * [
 *   {
 *     "faceId": 123,
 *     "faceImage": "data:image/jpeg,base64,..."
 *   },
 *   {
 *     "faceId": 456,
 *     "faceImage": "data:image/jpeg,base64,..."
 *   },
 *   {
 *     "faceId": 789,
 *     "faceImage": "data:image/jpeg,base64,..."
 *   }
 * ]
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 401 не авторизован
 * 500 ошибка на сервере
 */


/**
 * @api {post} /sgapi/sgUpdateGroup Обновить параметры специальной группы
 * @apiName sgUpdateGroup
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiUse Authorization
 *
 * @apiParam {String} callback URL для отправки событий о распознавании лиц (FRS --> callback)
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "callback": "https://host/faceRecognized"
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 401 не авторизован
 * 500 ошибка на сервере
 */


/**
 * @api {post} /sgapi/sgRenewToken Обновить токен авторизации специальной группы
 * @apiName sgRenewToken
 * @apiGroup Host-->FRS
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiUse Authorization
 *
 * @apiSuccess {String} accessApiToken новый токен авторизации для вызова API методов
 *
 * @apiSuccessExample {json} Пример результата выполнения
 * {
 *   "accessApiToken": "bd64b1ac-53ef-3ee5-fd2f-203d1e2d174e"
 * }
 *
 * @apiErrorExample {json} Ошибки
 * 400 некорректный запрос
 * 401 не авторизован
 * 500 ошибка на сервере
 */


/**
 * @api {post} callback Событие распознавания лица
 * @apiPrivate
 * @apiName faceRecognized
 * @apiGroup FRS-->Host
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {Number} faceId идентификатор дескриптора распознанного лица
 * @apiParam {Number} eventId идентификатор события из журнала FRS
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "faceId": 4567,
 *   "eventId": 12345
 * }
 */


/**
 * @api {post} callback Событие распознавания лица в специальной группе
 * @apiName sgFaceRecognized
 * @apiGroup FRS-->Host
 * @apiVersion 1.1.1
 *
 * @apiDescription **[в работе]**
 *
 * @apiParam {Number} faceId идентификатор дескриптора распознанного лица
 * @apiParam {String} screenshot URL кадра с видео потока
 * @apiParam {String="yyyy-MM-dd hh:mm:ss"} date дата события
 *
 * @apiParamExample {json} Пример использования
 * {
 *   "faceId": 4568,
 *   "screenshot": "http://localhost:9051/static/screenshots/9/c/e/9ce699cf4cff47570b28f372bf4bd4b3.jpg",
 *   "date": "2022-01-21 11:15:44"
 * }
 */

#pragma once

#include "absl/container/flat_hash_map.h"
#include "crow/app.h"
#include "singleton.h"

namespace API
{
  //константы
  constexpr const char* CONTENT_TYPE = "Content-Type";
  constexpr const char* MIME_TYPE = "application/json";
  inline constexpr absl::string_view AUTH_TYPE = "Bearer ";
  inline constexpr absl::string_view AUTH_HEADER = "Authorization";

  constexpr const char* MSG_DONE = "запрос выполнен";
  constexpr const char* MSG_SERVER_ERROR = "ошибка на сервере";

  //код ответа
  constexpr const int CODE_SUCCESS = 200;
  constexpr const int CODE_NO_CONTENT = 204;
  constexpr const int CODE_ERROR = 400;
  constexpr const int CODE_UNAUTHORIZED = 401;
  constexpr const int CODE_FORBIDDEN = 403;
  constexpr const int CODE_SERVER_ERROR = 500;

  //результат выполнения операции
  const HashMap<int, const char*> RESPONSE_RESULT =
  {
      {CODE_SUCCESS, "OK"}
    , {CODE_NO_CONTENT, "No Content"}
    , {CODE_ERROR, "Bad Request"}
    , {CODE_UNAUTHORIZED, "Unauthorized"}
    , {CODE_FORBIDDEN, "Forbidden"}
    , {CODE_SERVER_ERROR, "Internal Server Error"}
  };

  const HashMap<int, const char*> RESPONSE_MESSAGE =
  {
      {CODE_SUCCESS, MSG_DONE}
    , {CODE_NO_CONTENT, "нет содержимого"}
    , {CODE_ERROR, "некорректный запрос"}
    , {CODE_UNAUTHORIZED, "не авторизован"}
    , {CODE_FORBIDDEN, "запрещено"}
    , {CODE_SERVER_ERROR, MSG_SERVER_ERROR}
  };

  inline constexpr absl::string_view DATETIME_FORMAT = "%Y-%m-%d %H:%M:%S";
  inline constexpr absl::string_view DATETIME_FORMAT_LOG_FACES = "%Y-%m-%d %H:%M:%E3S";
  inline constexpr absl::string_view DATE_FORMAT = "%Y-%m-%d";
  inline constexpr absl::string_view INCORRECT_PARAMETER = "Некорректное значение параметра \"$0\" в запросе.";
  inline constexpr absl::string_view ERROR_INVALID_VSTREAM = "Не зарегистрирован видео поток с идентификатором $0.";
  inline constexpr absl::string_view ERROR_UNKNOWN_API_METHOD = "Неизвестная API функция в запросе.";
  inline constexpr absl::string_view ERROR_MISSING_PARAMETER = "Отсутствует параметр \"$0\" в запросе.";
  inline constexpr absl::string_view ERROR_NULL_PARAMETER = "Параметр \"$0\" равен null в запросе.";
  inline constexpr absl::string_view ERROR_EMPTY_VALUE = "Пустое значение у параметра \"$0\" в запросе.";
  inline constexpr absl::string_view ERROR_REQUEST_STRUCTURE = "Ошибка в структуре запроса.";
  inline constexpr absl::string_view ERROR_SGROUP_ALREADY_EXISTS = "Специальная группа с таким названием уже существует.";
  inline constexpr absl::string_view ERROR_SGROUP_MAX_DESCRIPTOR_COUNT_LIMIT = "Достигнуто максимально допустимое количество зарегистрированных дескрипторов: $0.";

  //запросы
  inline constexpr absl::string_view ADD_STREAM = "addStream";  //добавить или изменить видео поток
  inline constexpr absl::string_view MOTION_DETECTION = "motionDetection";  //информация о детекции движения
  inline constexpr absl::string_view DOOR_IS_OPEN = "doorIsOpen";  //уведомление об открытии двери домофона
  inline constexpr absl::string_view BEST_QUALITY = "bestQuality";  //получить информацию о кадре с "лучшим" лицом
  inline constexpr absl::string_view REGISTER_FACE = "registerFace";  //зарегистрировать лицо
  inline constexpr absl::string_view ADD_FACES = "addFaces";  //привязать дескрипторы к видео потоку
  inline constexpr absl::string_view REMOVE_FACES = "removeFaces";  //отвязать дескрипторы от видео потока
  inline constexpr absl::string_view LIST_STREAMS = "listStreams";  //список обрабатываемых видео потоков и дескрипторов
  inline constexpr absl::string_view REMOVE_STREAM = "removeStream";  //убрать видео поток
  inline constexpr absl::string_view LIST_ALL_FACES = "listAllFaces";  //список всех дескрипторов
  inline constexpr absl::string_view DELETE_FACES = "deleteFaces";  //удалить список дескрипторов из базы (в независимости от привязок к видео потокам)
  inline constexpr absl::string_view GET_EVENTS = "getEvents";  //получить список событий из временного интервала
  inline constexpr absl::string_view GET_SETTINGS = "getSettings";  //получить значения параметров
  inline constexpr absl::string_view SET_SETTINGS = "setSettings";  //установить значения параметров
  inline constexpr absl::string_view ADD_SPECIAL_GROUP = "addSpecialGroup";  //добавить специальную группу
  inline constexpr absl::string_view UPDATE_SPECIAL_GROUP = "updateSpecialGroup";  //обновить специальную группу
  inline constexpr absl::string_view DELETE_SPECIAL_GROUP = "deleteSpecialGroup";  //удалить специальную группу
  inline constexpr absl::string_view PROCESS_FRAME = "processFrame";  //обработать кадр по url
  inline constexpr absl::string_view SAVE_DNN_STATS_DATA = "saveDnnStatsData";  //сохранить данные статистики инференса
  inline constexpr absl::string_view SG_REGISTER_FACE = "sgRegisterFace";  //зарегистрировать лицо в специальной группе
  inline constexpr absl::string_view SG_DELETE_FACES = "sgDeleteFaces";  //удалить список дескрипторов специальной группы из базы
  inline constexpr absl::string_view SG_LIST_FACES = "sgListFaces";  //получить список всех дескрипторов специальной группы
  inline constexpr absl::string_view SG_UPDATE_GROUP = "sgUpdateGroup";  //обновить параметры специальной группы
  inline constexpr absl::string_view SG_RENEW_TOKEN = "sgRenewToken";  //обновить токен авторизации специальной группы

  //логи для вызова методов
  inline constexpr absl::string_view LOG_CALL_ADD_STREAM = "API call $0: streamId = $1;  url = $2;  callback = $3";
  inline constexpr absl::string_view LOG_CALL_MOTION_DETECTION = "API call $0: streamId = $1;  isStart = $2";
  inline constexpr absl::string_view LOG_CALL_DOOR_IS_OPEN = "API call $0: streamId = $1";
  inline constexpr absl::string_view LOG_CALL_BEST_QUALITY = "API call $0: eventId = $1;  streamId = $2;  date = $3;  uuid = $4";
  inline constexpr absl::string_view LOG_CALL_REGISTER_FACE =  "API call $0: streamId = $1;  url = $2;  face = [$3, $4, $5, $6]";
  inline constexpr absl::string_view LOG_CALL_ADD_OR_REMOVE_FACES = "API call $0: streamId = $1;  faceIds = [$2]";
  inline constexpr absl::string_view LOG_CALL_SIMPLE_METHOD = "API call $0";
  inline constexpr absl::string_view LOG_CALL_REMOVE_STREAM = "API call $0: streamId = $1";
  inline constexpr absl::string_view LOG_CALL_DELETE_FACES = "API call $0: faceIds = [$1]";
  inline constexpr absl::string_view LOG_CALL_GET_EVENTS = "API call $0: streamId = $1;  dateStart = $2;  dateEnd = $3";
  inline constexpr absl::string_view LOG_START_MOTION = "Зафиксировано движение. Начало захвата кадров с видео потока $0";
  inline constexpr absl::string_view LOG_TEST_IMAGE = "API call $0: streamId = $1;  url = $2";
  inline constexpr absl::string_view LOG_CALL_ADD_SPECIAL_GROUP = "API call $0: groupName = $1;  maxDescriptorCount = $2";
  inline constexpr absl::string_view LOG_CALL_UPDATE_SPECIAL_GROUP = "API call $0: id_sgroup = $1;  groupName = $2;  maxDescriptorCount = $3";
  inline constexpr absl::string_view LOG_CALL_PROCESS_FRAME = "API call $0: streamId = $1;  groupId = $2;  url = $3";
  inline constexpr absl::string_view LOG_CALL_SG_SIMPLE_METHOD = "API call $0: id_sgroup = $1";
  inline constexpr absl::string_view LOG_CALL_SG_REGISTER_FACE =  "API call $0: id_sgroup = $1;  url = $2;  face = [$3, $4, $5, $6]";
  inline constexpr absl::string_view LOG_CALL_SG_DELETE_FACES = "API call $0: id_sgroup = $1;  faceIds = [$2]";
  inline constexpr absl::string_view LOG_CALL_SG_UPDATE_GROUP = "API call $0: id_sgroup = $1;  callback = $2";

  //для теста
  inline constexpr absl::string_view TEST_IMAGE = "testImage";  //протестировать изображение
  inline constexpr absl::string_view TEST_CALLBACK = "testCallback";  //протестировать callback

  //для переноса дескрипторов используем виртуальный видео поток, который не показываем в методе listStreams
  inline constexpr absl::string_view VIRTUAL_VS = "virtual";

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
  constexpr const char* P_EVENT_UUID = "uuid";
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
  constexpr const char* P_PARAM_TYPE = "paramType";
  constexpr const char* P_PARAM_COMMENTS = "description";
  constexpr const char* P_PARAM_MIN_VALUE = "minValue";
  constexpr const char* P_PARAM_MAX_VALUE = "maxValue";
  constexpr const char* P_QUALITY = "quality";
  constexpr const char* P_DATE_START = "dateStart";
  constexpr const char* P_DATE_END = "dateEnd";
  constexpr const char* P_SPECIAL_GROUP_NAME = "groupName";
  constexpr const char* P_MAX_DESCRIPTOR_COUNT = "maxDescriptorCount";
  constexpr const char* P_GROUP_ID = "groupId";
  constexpr const char* P_SG_API_TOKEN = "accessApiToken";
  constexpr const char* P_SG_ID = "groupId";
}

class ApiService : public crow::SimpleApp
{
public:
  static void handleRequest(const crow::request& request, crow::response& response, const String& api_method);
  static void handleSGroupRequest(const crow::request& request, crow::response& response, const String& api_method_sgroup);
  virtual ~ApiService();

private:
  static bool checkInputParam(const crow::json::rvalue& json, crow::response& response, const char* input_param);
  static void simpleResponse(int code, crow::response& response);
  static void simpleResponse(int code, const String& msg, crow::response& response);

  //api функции
  static bool addVStream(const String& vstream_ext, const String& url, const String& callback_url, const std::vector<int>& face_ids,
    const HashMap<String, String>& params);
  static void motionDetection(const String& vstream_ext, bool is_start, bool is_door_open = false);
  static bool addFaces(const String& vstream_ext, const std::vector<int>& face_ids);
  static bool removeFaces(const String& vstream_ext, const std::vector<int>& face_ids);
  static bool deleteFaces(const std::vector<int>& face_ids);
  static bool removeVStream(const String& vstream_ext);
  static std::tuple<int, String> addSpecialGroup(const String& group_name, int max_descriptor_count);
  static bool updateSpecialGroup(int id_sgroup, const String& group_name, int max_descriptor_count);
  static bool sgDeleteFaces(int id_sgroup, const std::vector<int>& face_ids);
  static bool setParams(const HashMap<String, String>& params);
};
