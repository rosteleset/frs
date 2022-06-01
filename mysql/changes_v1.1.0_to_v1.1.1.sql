--
-- Изменения в базе данных FRS при переходе c версии 1.1.0 на 1.1.1
--

-- Новые параметры:
INSERT INTO common_settings (param_name, param_value, param_comments, for_vstream) VALUES ('open-door-duration', '5.0', 'Время открытия двери в секундах', 1);
