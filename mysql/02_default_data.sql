-- Параметры в таблице common_settings
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('alpha', '1.0', 'Альфа-коррекция', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('best-quality-interval-after', '2.0', 'Период в секундах после временной точки для поиска лучшего кадра', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('best-quality-interval-before', '5.0', 'Период в секундах до временной точки для поиска лучшего кадра', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('beta', '0.0', 'Бета-коррекция', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('blur', '300', 'Нижний порог размытости лица', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('blur-max', '13000', 'Верхний порог размытости лица', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('callback-timeout', '2', 'Время ожидания ответа при вызове callback URL (в секундах)', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('capture-timeout', '2', 'Время ожидания захвата кадра с камеры в секундах', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('clear-old-log-faces', '4', 'Период запуска очистки устаревших сохраненных логов (в часах)', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-blurry-face', 'Изображение лица недостаточно четкое для регистрации.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-descriptor-creation-error', 'Не удалось зарегистрировать дескриптор.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-descriptor-exists', 'Дескриптор уже существует.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-inference-error', 'Ошибка: не удалось сделать запрос Triton Inference Server.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-new-descriptor', 'Создан новый дескриптор.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-no-faces', 'Нет лиц на изображении.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-non-frontal-face', 'Лицо на изображении должно быть фронтальным.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-non-normal-face-class', 'Лицо в маске или в темных очках.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-partial-face', 'Лицо должно быть полностью видимым на изображении.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('comments-url-image-error', 'Не удалось получить изображение.', null, 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('delay-between-frames', '1.0', 'Интервал в секундах между захватами кадров', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('descriptor-inactivity-period', '400', 'Период неактивности дескриптора в днях, спустя который он удаляется', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-inference-server', 'localhost:8000', 'Сервер для инференса получения класса лица (нормальное, в маске, в темных очках', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-input-height', '192', 'Высота изображения для получения класса лица', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-input-tensor-name', 'input.1', 'Название входного тензора для получения класса лица', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-input-width', '192', 'Ширина изображения для получения класса лица', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-model-name', 'genet', 'Название модели нейронной сети для получения класса лица', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-output-size', '3', 'Количество элементов выходного массива для получения класса лица', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fc-output-tensor-name', '419', 'Название выходного тензора для получения класса лица', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fd-inference-server', 'localhost:8000', 'Сервер для инференса детекции лиц', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fd-input-height', '320', 'Высота изображения для детекции лиц', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fd-input-tensor-name', 'input.1', 'Название входного тензора для детекции лиц', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fd-input-width', '320', 'Ширина изображения для детекции лиц', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fd-model-name', 'scrfd', 'Название модели нейронной сети для детекции лиц', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-inference-server', 'localhost:8000', 'Сервер для инференса получения дескриптора', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-input-height', '112', 'Высота изображения для получения дескриптора', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-input-tensor-name', 'input.1', 'Название входного тензора для получения дескриптора', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-input-width', '112', 'Ширина изображения для получения дескриптора', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-model-name', 'arcface', 'Название модели нейронной сети для получения дескриптора', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-output-size', '512', 'Количество элементов выходного массива для получения дескриптора', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('dnn-fr-output-tensor-name', '683', 'Название выходного тензора для получения дескриптора', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('face-class-confidence', '0.7', 'Порог вероятности определения маски на лице', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('face-confidence', '0.75', 'Порог вероятности определения лица', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('face-enlarge-scale', '1.5', 'Коэффициент расширения прямоугольной области лица для хранения', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('flag-copy-event-data', '1', 'Флаг копирования данных события в отдельную директорию', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('gamma', '1', 'Гамма-коррекция', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('log-faces-live-interval', '12', '"Время жизни" логов из таблицы log_faces (в часах)', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('logs-level', '1', 'уровень логов: 0 - ошибки; 1 - события; 2 - подробно', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('margin', '5', 'Отступ в процентах от краев кадра для уменьшения рабочей области', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('max-capture-error-count', '3', 'Максимальное количество подряд полученных ошибок при получении кадра с камеры, после которого будет считаться попытка получения кадра неудачной', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('open-door-duration', '5.0', 'Время открытия двери в секундах', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('pitch-threshold', '30', 'Пороговое значение тангажа (нос вверх-вниз). Диапазон от 0.01 до 70 градусов', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('process-frames-interval', '0.0', 'Интервал в секундах, втечение которого обрабатываются кадры после окончания детекции движения', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('retry-pause', '30', 'Пауза в секундах перед следующей попыткой захвата кадра после неудачи', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('roll-threshold', '45.0', 'Пороговые значения для ориентации лица, при которых лицо будет считаться фронтальным. Диапазон от 0.01 до 70 градусов', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('tolerance', '0.5', 'Параметр толерантности', 1);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('yaw-threshold', '45', 'Пороговое значение рысканья (нос влево-вправо). Диапазон от 0.01 до 70 градусов', 1);


-- Данные в таблице workers
INSERT INTO workers (id_worker, url) VALUES (1, '');
