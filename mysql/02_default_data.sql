-- MySQL dump 10.13  Distrib 8.0.26, for Linux (x86_64)
--
-- Host: faceid.lanta.me    Database: db_frs
-- ------------------------------------------------------
-- Server version	8.0.26-0ubuntu0.20.04.2

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `common_settings`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `common_settings` (
  `param_name` varchar(100) NOT NULL COMMENT 'Название параметра',
  `param_value` varchar(100) DEFAULT NULL COMMENT 'Значение параметра',
  `param_comments` varchar(200) DEFAULT NULL COMMENT 'Описание параметра',
  `for_vstream` tinyint NOT NULL DEFAULT '1' COMMENT 'Признак параметра для видеопотока',
  `time_point` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Время последнего изменения',
  PRIMARY KEY (`param_name`),
  KEY `common_settings_time_point_IDX` (`time_point`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Настройки программы';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `common_settings`
--

INSERT INTO `common_settings` VALUES ('alpha','1.0','Альфа-коррекция',1,'2020-10-07 08:32:19'),('best-quality-interval-after','2.0','Период в секундах после временной точки для поиска лучшего кадра',1,'2021-05-19 12:26:57'),('best-quality-interval-before','5.0','Период в секундах до временной точки для поиска лучшего кадра',1,'2021-05-19 12:26:22'),('beta','0.0','Бета-коррекция',1,'2020-10-07 08:32:17'),('blur','300','Нижний порог размытости лица',1,'2020-11-02 15:14:35'),('blur-max','13000','Верхний порог размытости лица',1,'2020-11-02 15:14:35'),('capture-timeout','2','Время ожидания захвата кадра с камеры в секундах',1,'2020-10-07 08:32:17'),('clear-old-log-faces','4','Период запуска очистки устаревших сохраненных логов (в часах)',0,'2021-04-30 07:33:12'),('comments-blurry-face','Изображение лица недостаточно четкое для регистрации.',NULL,1,'2021-05-20 15:02:13'),('comments-descriptor-creation-error','Не удалось зарегистрировать дескриптор.',NULL,1,'2021-05-20 15:02:13'),('comments-descriptor-exists','Дескриптор уже существует.',NULL,1,'2021-05-20 15:02:13'),('comments-inference-error','Ошибка: не удалось сделать запрос Triton Inference Server.',NULL,1,'2021-05-20 15:02:13'),('comments-new-descriptor','Создан новый дескриптор.',NULL,1,'2021-05-20 15:02:13'),('comments-no-faces','Нет лиц на изображении.',NULL,1,'2021-05-20 15:02:13'),('comments-non-frontal-face','Лицо на изображении должно быть фронтальным.',NULL,1,'2021-05-20 15:02:13'),('comments-non-normal-face-class','Лицо в маске или в темных очках.',NULL,1,'2021-07-05 13:26:20'),('comments-partial-face','Лицо должно быть полностью видимым на изображении.',NULL,1,'2021-05-20 15:02:13'),('comments-url-image-error','Не удалось получить изображение.',NULL,1,'2021-05-20 15:02:13'),('delay-between-frames','1.0','Интервал в секундах между захватами кадров',1,'2020-10-07 08:32:17'),('descriptor-inactivity-period','400','Период неактивности дескриптора в днях, спустя который он удаляется',1,'2020-10-07 08:32:17'),('dnn-fc-inference-server','localhost:8000','Сервер для инференса получения класса лица (нормальное, в маске, в темных очках',1,'2021-07-05 08:22:19'),('dnn-fc-input-height','192','Высота изображения для получения класса лица',0,'2021-07-05 08:22:19'),('dnn-fc-input-tensor-name','input.1','Название входного тензора для получения класса лица',0,'2021-07-05 08:22:19'),('dnn-fc-input-width','192','Ширина изображения для получения класса лица',0,'2021-07-05 08:22:19'),('dnn-fc-model-name','genet','Название модели нейронной сети для получения класса лица',0,'2021-07-05 08:22:19'),('dnn-fc-output-size','3','Количество элементов выходного массива для получения класса лица',0,'2021-07-05 08:22:19'),('dnn-fc-output-tensor-name','419','Название выходного тензора для получения класса лица',0,'2021-07-05 08:22:19'),('dnn-fd-inference-server','localhost:8000','Сервер для инференса детекции лиц',1,'2021-05-27 13:34:38'),('dnn-fd-input-height','320','Высота изображения для детекции лиц',0,'2021-05-27 13:34:38'),('dnn-fd-input-tensor-name','input.1','Название входного тензора для детекции лиц',0,'2021-05-27 13:34:38'),('dnn-fd-input-width','320','Ширина изображения для детекции лиц',0,'2021-05-27 13:34:38'),('dnn-fd-model-name','scrfd','Название модели нейронной сети для детекции лиц',0,'2021-05-27 13:34:38'),('dnn-fr-inference-server','localhost:8000','Сервер для инференса получения дескриптора',1,'2021-05-27 13:34:38'),('dnn-fr-input-height','112','Высота изображения для получения дескриптора',0,'2021-05-27 13:34:38'),('dnn-fr-input-tensor-name','input.1','Название входного тензора для получения дескриптора',0,'2021-05-27 13:34:38'),('dnn-fr-input-width','112','Ширина изображения для получения дескриптора',0,'2021-05-27 13:34:38'),('dnn-fr-model-name','arcface','Название модели нейронной сети для получения дескриптора',0,'2021-05-27 13:34:38'),('dnn-fr-output-size','512','Количество элементов выходного массива для получения дескриптора',0,'2021-05-27 13:34:38'),('dnn-fr-output-tensor-name','683','Название выходного тензора для получения дескриптора',0,'2021-05-27 13:34:38'),('face-class-confidence','0.7','Порог вероятности определения маски на лице',1,'2021-05-11 06:39:32'),('face-confidence','0.75','Порог вероятности определения лица',1,'2021-05-08 08:00:16'),('face-enlarge-scale','1.5','Коэффициент расширения прямоугольной области лица для хранения',1,'2020-10-07 08:32:17'),('gamma','1','Гамма-коррекция',1,'2020-10-07 08:32:17'),('log-faces-live-interval','12','\"Время жизни\" логов из таблицы log_faces (в часах)',0,'2021-05-27 14:57:36'),('logs-level','1','уровень логов: 0 - ошибки; 1 - события; 2 - подробно',1,'2020-10-07 15:35:22'),('margin','5','Отступ в процентах от краев кадра для уменьшения рабочей области',1,'2020-10-07 08:32:17'),('max-capture-error-count','3','Максимальное количество подряд полученных ошибок при получении кадра с камеры, после которого будет считаться попытка получения кадра неудачной',1,'2020-10-07 08:32:17'),('pitch-threshold','30','Пороговое значение тангажа (нос вверх-вниз). Диапазон от 0.01 до 70 градусов',1,'2020-10-07 08:32:17'),('process-frames-interval','0.0','Интервал в секундах, втечение которого обрабатываются кадры после окончания детекции движения',1,'2020-10-07 08:32:17'),('retry-pause','30','Пауза в секундах перед следующей попыткой захвата кадра после неудачи',1,'2020-10-08 12:04:02'),('roll-threshold','45.0','Пороговые значения для ориентации лица, при которых лицо будет считаться фронтальным. Диапазон от 0.01 до 70 градусов',1,'2020-10-07 08:32:17'),('tolerance','0.5','Параметр толерантности',1,'2020-12-15 08:00:26'),('yaw-threshold','45','Пороговое значение рысканья (нос влево-вправо). Диапазон от 0.01 до 70 градусов',1,'2020-10-07 08:32:17');

--
-- Table structure for table `workers`
--

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `workers` (
  `id_worker` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор рабочего сервера FRS',
  `url` varchar(200) NOT NULL COMMENT 'URL рабочего сервера FRS',
  PRIMARY KEY (`id_worker`),
  UNIQUE KEY `workers_UN` (`url`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Рабочие сервера FRS';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `workers`
--

INSERT INTO `workers` VALUES (1,'');
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2021-10-18 13:33:29
