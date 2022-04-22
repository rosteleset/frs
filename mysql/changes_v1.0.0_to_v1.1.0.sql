--
-- Изменения в структуре базы данных FRS при переходе c версии 1.0.0 на 1.1.0
--

-- Новая таблица special_groups
DROP TABLE IF EXISTS `special_groups`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `special_groups` (
  `id_special_group` int NOT NULL AUTO_INCREMENT COMMENT 'Идентификатор специальной группы',
  `group_name` varchar(200) NOT NULL COMMENT 'Название специальной группы',
  `sg_api_token` varchar(64) NOT NULL COMMENT 'Токен для работы с api методами специальной группы',
  `callback_url` varchar(200) DEFAULT NULL COMMENT 'URL для вызова при возникновении события распознавания лица',
  `max_descriptor_count` int NOT NULL DEFAULT '1000' COMMENT 'Максимальное количество дескрипторов в специальной группе',
  PRIMARY KEY (`id_special_group`),
  UNIQUE KEY `special_groups_group_name_uindex` (`group_name`),
  UNIQUE KEY `special_groups_sg_api_token_uindex` (`sg_api_token`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Специальные группы с обработкой на всех видео потоках';
/*!40101 SET character_set_client = @saved_cs_client */;


-- Новая таблица link_descriptor_sgroup
DROP TABLE IF EXISTS `link_descriptor_sgroup`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `link_descriptor_sgroup` (
  `id_descriptor` int NOT NULL,
  `id_sgroup` int NOT NULL,
  UNIQUE KEY `link_descriptor_sgroup_id_descriptor_id_sgroup_uindex` (`id_descriptor`,`id_sgroup`),
  KEY `link_descriptor_sgroup_special_groups_id_special_group_fk` (`id_sgroup`),
  CONSTRAINT `link_descriptor_sgroup_face_descriptors_id_descriptor_fk` FOREIGN KEY (`id_descriptor`) REFERENCES `face_descriptors` (`id_descriptor`) ON DELETE CASCADE ON UPDATE CASCADE,
  CONSTRAINT `link_descriptor_sgroup_special_groups_id_special_group_fk` FOREIGN KEY (`id_sgroup`) REFERENCES `special_groups` (`id_special_group`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Привязка дескриптора к специальной группе';
/*!40101 SET character_set_client = @saved_cs_client */;


-- Изменения в таблице log_faces
create index log_faces_FK2 on log_faces (id_vstream);
alter table log_faces drop key log_IDX;
alter table log_faces add constraint log_IDX unique (id_vstream, log_date, id_descriptor);


-- Новые параметры в таблице common_settings
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('callback-timeout', '2', 'Время ожидания ответа при вызове callback URL (в секундах)', 0);
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('flag-copy-event-data', '1', 'Флаг копирования данных события в отдельную директорию', 0);
