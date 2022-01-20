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

DROP TABLE IF EXISTS `common_settings`;
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
-- Table structure for table `descriptor_images`
--

DROP TABLE IF EXISTS `descriptor_images`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `descriptor_images` (
  `id_descriptor` int NOT NULL COMMENT 'Идентификатор дескриптора',
  `mime_type` varchar(50) DEFAULT NULL COMMENT 'Тип изображения',
  `face_image` mediumblob COMMENT 'Изображение лица',
  PRIMARY KEY (`id_descriptor`),
  CONSTRAINT `descriptor_images_FK` FOREIGN KEY (`id_descriptor`) REFERENCES `face_descriptors` (`id_descriptor`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Изображения дескрипторов';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `face_descriptors`
--

DROP TABLE IF EXISTS `face_descriptors`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `face_descriptors` (
  `id_descriptor` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор дескриптора',
  `descriptor_data` varbinary(2048) DEFAULT NULL COMMENT 'Дескриптор лица (вектор)',
  `date_start` datetime DEFAULT CURRENT_TIMESTAMP COMMENT 'Дата начала использования дескриптора',
  `date_last` datetime DEFAULT CURRENT_TIMESTAMP COMMENT 'Дата последнего использования дескриптора',
  UNIQUE KEY `person_descriptors_descriptor_IDX` (`id_descriptor`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=118873 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Биометрические парметры (дескрипторы) лиц';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `link_descriptor_vstream`
--

DROP TABLE IF EXISTS `link_descriptor_vstream`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `link_descriptor_vstream` (
  `id_descriptor` int DEFAULT NULL COMMENT 'Идентификатор дескриптора',
  `id_vstream` int DEFAULT NULL COMMENT 'Идентификатор видеопотока',
  UNIQUE KEY `link_person_vstream_UN` (`id_descriptor`,`id_vstream`),
  KEY `link_person_vstream_FK_1` (`id_vstream`),
  CONSTRAINT `link_person_vstream_FK` FOREIGN KEY (`id_descriptor`) REFERENCES `face_descriptors` (`id_descriptor`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `link_person_vstream_FK_1` FOREIGN KEY (`id_vstream`) REFERENCES `video_streams` (`id_vstream`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Привязка дескриптора к видеопотоку';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `link_worker_vstream`
--

DROP TABLE IF EXISTS `link_worker_vstream`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `link_worker_vstream` (
  `id_worker` int NOT NULL COMMENT 'Идентификатор рабочего сервера FRS',
  `id_vstream` int NOT NULL COMMENT 'Идентификатор видео потока',
  PRIMARY KEY (`id_worker`,`id_vstream`),
  UNIQUE KEY `link_worker_vstream_UN` (`id_vstream`),
  CONSTRAINT `link_worker_vstream_FK` FOREIGN KEY (`id_worker`) REFERENCES `workers` (`id_worker`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `link_worker_vstream_FK_1` FOREIGN KEY (`id_vstream`) REFERENCES `video_streams` (`id_vstream`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE TRIGGER `trg_on_remove_link2` AFTER DELETE ON `link_worker_vstream` FOR EACH ROW if not exists(select * from link_worker_vstream where id_vstream = old.id_vstream) then
    delete from video_streams where id_vstream = old.id_vstream;
  end if */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;

--
-- Table structure for table `log_delivery_events`
--

DROP TABLE IF EXISTS `log_delivery_events`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `log_delivery_events` (
  `id_log` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор записи в журнале',
  `delivery_type` tinyint NOT NULL DEFAULT '1' COMMENT 'Тип отправки:\n1 - лицо распознано',
  `delivery_result` tinyint NOT NULL DEFAULT '1' COMMENT 'Результат доставки события:\n0 - ошибка;\n1 - доставлено.',
  `id_vstream` int NOT NULL COMMENT 'Идентификатор видеопотока',
  `id_descriptor` int NOT NULL COMMENT 'Идентификатор дескриптора',
  `log_date` datetime(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT 'Дата записи в журнал',
  PRIMARY KEY (`id_log`),
  UNIQUE KEY `log_delivery_events_id_vstream_IDX` (`id_vstream`,`id_descriptor`,`log_date`) USING BTREE,
  KEY `log_delivery_events_log_date_IDX` (`log_date`) USING BTREE,
  KEY `log_delivery_events_id_descriptor_IDX` (`id_descriptor`,`log_date`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=595490 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Журнал доставки событий';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `log_faces`
--

DROP TABLE IF EXISTS `log_faces`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `log_faces` (
  `id_log` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор записи в журнале',
  `id_vstream` int NOT NULL COMMENT 'Идентификатор видеопотока',
  `log_date` datetime(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT 'Дата и время записи в журнал',
  `id_descriptor` int DEFAULT NULL COMMENT 'Идентификатор дескриптора',
  `quality` double DEFAULT NULL COMMENT 'Качество лица на скриншоте',
  `screenshot` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'Относительный путь к файлу со скриншотом',
  `face_left` int DEFAULT NULL COMMENT 'Координата X прямоугольной области лица',
  `face_top` int DEFAULT NULL COMMENT 'Координата Y прямоугольной области лица',
  `face_width` int DEFAULT NULL COMMENT 'Ширина прямоугольной области лица',
  `face_height` int DEFAULT NULL COMMENT 'Высота прямоугольной области лица',
  PRIMARY KEY (`id_log`),
  UNIQUE KEY `log_IDX` (`id_vstream`,`log_date`) USING BTREE,
  KEY `log_date_IDX` (`log_date`) USING BTREE,
  KEY `log_faces_FK` (`id_descriptor`),
  CONSTRAINT `log_faces_FK` FOREIGN KEY (`id_descriptor`) REFERENCES `face_descriptors` (`id_descriptor`) ON DELETE SET NULL ON UPDATE CASCADE,
  CONSTRAINT `log_FK` FOREIGN KEY (`id_vstream`) REFERENCES `video_streams` (`id_vstream`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=5285382 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Журнал событий лиц';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `video_stream_settings`
--

DROP TABLE IF EXISTS `video_stream_settings`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `video_stream_settings` (
  `id_vstream` int NOT NULL COMMENT 'Идентификатор видеопотока',
  `param_name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Название параметра',
  `param_value` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Значение параметра',
  UNIQUE KEY `video_stream_settings_UN` (`param_name`,`id_vstream`),
  KEY `video_stream_settings_FK_1` (`id_vstream`),
  CONSTRAINT `video_stream_settings_FK` FOREIGN KEY (`param_name`) REFERENCES `common_settings` (`param_name`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `video_stream_settings_FK_1` FOREIGN KEY (`id_vstream`) REFERENCES `video_streams` (`id_vstream`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Переопределенные параметры для камер';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `video_streams`
--

DROP TABLE IF EXISTS `video_streams`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `video_streams` (
  `id_vstream` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор видеопотока',
  `vstream_ext` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'Внешний идентификатор видеопотока',
  `url` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'URL для захвата кадров',
  `callback_url` varchar(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT 'URL для вызова методов',
  `region_x` int DEFAULT '0' COMMENT 'Координата X рабочей области кадра',
  `region_y` int DEFAULT '0' COMMENT 'Координата Y рабочей области кадра',
  `region_width` int DEFAULT '0' COMMENT 'Ширина рабочей области кадра',
  `region_height` int DEFAULT '0' COMMENT 'Высота рабочей области кадра',
  PRIMARY KEY (`id_vstream`),
  UNIQUE KEY `video_streams_UN` (`vstream_ext`)
) ENGINE=InnoDB AUTO_INCREMENT=1248 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Информация о видеопотоках';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE TRIGGER `trg_before_delete_vstream` BEFORE DELETE ON `video_streams` FOR EACH ROW delete from link_descriptor_vstream where id_vstream = old.id_vstream */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;

--
-- Table structure for table `workers`
--

DROP TABLE IF EXISTS `workers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `workers` (
  `id_worker` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор рабочего сервера FRS',
  `url` varchar(200) NOT NULL COMMENT 'URL рабочего сервера FRS',
  PRIMARY KEY (`id_worker`),
  UNIQUE KEY `workers_UN` (`url`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Рабочие сервера FRS';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2021-10-18 13:30:22
