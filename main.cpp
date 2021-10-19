#include <csignal>
#include <Wt/WServer.h>
#include <Wt/WLogger.h>

#include "frs_api.h"
#include "singleton.h"

bool checkConfig(QHash<QString, QString>& config)
{
  Singleton& singleton = Singleton::instance();

  bool is_config_ok = true;
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    const auto& k = it.key();
    if (singleton.task_config.conf_params.contains(k))
    {
      bool is_ok = true;
      switch (singleton.task_config.conf_params[k].value.type())
      {
        case QVariant::Bool:
          singleton.task_config.conf_params[k].value = bool(config[k].toInt(&is_ok));
          break;
        case QVariant::Int:
          singleton.task_config.conf_params[k].value = config[k].toInt(&is_ok);
          break;
        case QVariant::Double:
          singleton.task_config.conf_params[k].value = config[k].toDouble(&is_ok);
          break;
        default:
          singleton.task_config.conf_params[k].value = config[k];
          break;
      }

      //проверка на диапазон значений
      if (is_ok
          && (singleton.task_config.conf_params[k].value.type() == QVariant::Bool
            || singleton.task_config.conf_params[k].value.type() == QVariant::Int
            || singleton.task_config.conf_params[k].value.type() == QVariant::Double))
      {
        bool is_ok_range = true;
        QString s_range;
        auto d1 = singleton.task_config.conf_params[k].min_value;
        auto d2 = singleton.task_config.conf_params[k].max_value;
        if (d1.isValid() && d2.isValid())
        {
          is_ok_range = (singleton.task_config.conf_params[k].value >= d1 && singleton.task_config.conf_params[k].value <= d2);
          s_range = QString("в диапазоне [%1; %2]").arg(d1.toString(), d2.toString());
        }
        if (d1.isValid() && !d2.isValid())
        {
          is_ok_range = (singleton.task_config.conf_params[k].value >= d1);
          s_range = QString("больше или равно %1").arg(d1.toString());
        }
        if (!d1.isValid() && d2.isValid())
        {
          is_ok_range = (singleton.task_config.conf_params[k].value <= d2);
          s_range = QString("меньше или равно %1").arg(d2.toString());
        }

        if (!is_ok_range)
        {
          cerr << qUtf8Printable(QString("Значение параметра %1 (указано \"%2\") должно быть %3.\n").arg(k, singleton.task_config.conf_params[k].value.toString(), s_range));
          is_config_ok = false;
        }
      }

      if (!is_ok)
      {
        cerr << qUtf8Printable(QString("Неправильно указано значение параметра %1: %2\n").arg(k, config[k]));
        is_config_ok = false;
      }
    } else
    {
      cerr << qUtf8Printable(QString("Предупреждение: в конфигурации указан неизвестный параметр %1\n").arg(k));
    }
  }

  return is_config_ok;
}

int main(int argc, char** argv)
{
  Singleton& singleton = Singleton::instance();
  //singleton.argc = argc;
  //singleton.argv = argv;

  //конфигурационные параметры со значениями по-умолчанию
  singleton.task_config.conf_params = {
      {CONF_FILE, {".config", "Файл с конфигурацией"}}
    , {CONF_LOGS_LEVEL, {2, "Уровень логов", 0, 2}}
    , {CONF_FACE_CONFIDENCE_THRESHOLD, {0.8, "Порог вероятности определения лица", 0.1, 1.0}}
    , {CONF_FACE_CLASS_CONFIDENCE_THRESHOLD, {0.7, "Порог вероятности определения маски или темных очков на лице", 0.001, 1.0}}
    , {CONF_LAPLACIAN_THRESHOLD, {50.0, "Нижний порог размытости лица", 0.0, 100000.0}}
    , {CONF_LAPLACIAN_THRESHOLD_MAX, {15000.0, "Верхний порог размытости лица", 0.0, 100000.0}}
    , {CONF_TOLERANCE, {0.5, "Параметр толерантности", 0.1, 1.0}}
    , {CONF_MAX_CAPTURE_ERROR_COUNT, {3, "Максимальное количество подряд полученных ошибок при получении кадра с камеры, после которого будет считаться попытка получения кадра неудачной", 1, 10}}
    , {CONF_RETRY_PAUSE, {30, "Пауза в секундах между попытками установки связи с видеопотоком", 3, 10 * 60 *60}}
    , {CONF_FACE_ENLARGE_SCALE, {1.4, "Коэффициент расширения прямоугольной области лица для хранения", 1.0, 2.0}}
    , {CONF_ALPHA, {1.0, "Альфа-коррекция", 0.0, 1.0}}
    , {CONF_BETA, {0.0, "Бета-коррекция", 0.0, 255.0}}
    , {CONF_GAMMA, {1.0, "Гамма-коррекция", 0.1, 100.0}}
    , {CONF_CAPTURE_TIMEOUT, {1.0, "Время ожидания захвата кадра с камеры в секундах", 0.1, 5.0}}
    , {CONF_MARGIN, {5.0, "Отступ в процентах от краев кадра для уменьшения рабочей области", 0.0, 99.0}}
    , {CONF_DESCRIPTOR_INACTIVITY_PERIOD, {60, "Период неактивности дескриптора в днях, спустя который он удаляется", 1, INT_MAX}}
    , {CONF_ROLL_THRESHOLD, {45.0, "Пороговые значения для ориентации лица, при которых лицо будет считаться фронтальным. Диапазон от 0.01 до 70 градусов", 0.01, 70.0}}
    , {CONF_YAW_THRESHOLD, {15.0, "Пороговое значение рысканья (нос влево-вправо). Диапазон от 0.01 до 70 градусов", 0.01, 70.0}}
    , {CONF_PITCH_THRESHOLD, {30.0, "Пороговое значение тангажа (нос вверх-вниз). Диапазон от 0.01 до 70 градусов", 0.01, 70.0}}
    , {CONF_BEST_QUALITY_INTERVAL_BEFORE, {5.0, "Период в секундах до временной точки для поиска лучшего кадра", 0.5, 10.0}}
    , {CONF_BEST_QUALITY_INTERVAL_AFTER, {2.0, "Период в секундах после временной точки для поиска лучшего кадра", 0.5, 10.0}}

    //для детекции движения
    , {CONF_PROCESS_FRAMES_INTERVAL, {1.0, "Интервал в секундах, втечение которого обрабатываются кадры после окончания детекции движения", 0.0, 30.0}}
    , {CONF_DELAY_BETWEEN_FRAMES, {1.0, "Интервал в секундах между захватами кадров после детекции движения", 0.3, 5.0}}

    //для периодического удаления устаревших логов из таблицы log_ext_events
    , {CONF_CLEAR_OLD_LOG_FACES, {8.0, "Период запуска очистки устаревших сохраненных логов (в часах)", 0.0, 24.0 * 365}}
    , {CONF_LOG_FACES_LIVE_INTERVAL, {24.0, "\"Время жизни\" логов из таблицы log_faces (в часах)", 0.0, 24.0 * 365}}

    //для комментариев API методов
    , {CONF_COMMENTS_NO_FACES, {"Нет лиц на изображении."}}
    , {CONF_COMMENTS_PARTIAL_FACE, {"Лицо должно быть полностью видно на изображении."}}
    , {CONF_COMMENTS_NON_FRONTAL_FACE, {"Лицо на изображении должно быть фронтальным."}}
    , {CONF_COMMENTS_NON_NORMAL_FACE_CLASS, {"Лицо в маске или в темных очках."}}
    , {CONF_COMMENTS_BLURRY_FACE, {"Изображение лица недостаточно четкое для регистрации."}}
    , {CONF_COMMENTS_NEW_DESCRIPTOR, {"Создан новый дескриптор."}}
    , {CONF_COMMENTS_DESCRIPTOR_EXISTS, {"Дескриптор уже существует."}}
    , {CONF_COMMENTS_DESCRIPTOR_CREATION_ERROR, {"Не удалось зарегистрировать дескриптор."}}
    , {CONF_COMMENTS_INFERENCE_ERROR, {"Не удалось зарегистрировать дескриптор."}}
    , {CONF_COMMENTS_URL_IMAGE_ERROR, {"Не удалось получить изображение."}}

    //для инференса
    , {CONF_DNN_FD_INFERENCE_SERVER, {"localhost:8000"}}
    , {CONF_DNN_FD_MODEL_NAME, {"scrfd"}}
    , {CONF_DNN_FD_INPUT_TENSOR_NAME, {"input.1"}}
    , {CONF_DNN_FD_INPUT_WIDTH, {320}}
    , {CONF_DNN_FD_INPUT_HEIGHT, {320}}

    , {CONF_DNN_FC_INFERENCE_SERVER, {"localhost:8000"}}
    , {CONF_DNN_FC_MODEL_NAME, {"genet"}}
    , {CONF_DNN_FC_INPUT_TENSOR_NAME, {"input.1"}}
    , {CONF_DNN_FC_INPUT_WIDTH, {192}}
    , {CONF_DNN_FC_INPUT_HEIGHT, {192}}
    , {CONF_DNN_FC_OUTPUT_TENSOR_NAME, {"419"}}
    , {CONF_DNN_FC_OUTPUT_SIZE, {3}}

    , {CONF_DNN_FR_INFERENCE_SERVER, {"localhost:8000"}}
    , {CONF_DNN_FR_MODEL_NAME, {"arcface"}}
    , {CONF_DNN_FR_INPUT_TENSOR_NAME, {"input.1"}}
    , {CONF_DNN_FR_INPUT_WIDTH, {112}}
    , {CONF_DNN_FR_INPUT_HEIGHT, {112}}
    , {CONF_DNN_FR_OUTPUT_TENSOR_NAME, {"683"}}
    , {CONF_DNN_FR_OUTPUT_SIZE, {512}}
  };

  QStringList args;
  for (int i = 0; i < argc; ++i)
  {
    args << argv[i];
    if (QString(argv[i]) == "--frs-config" && i < argc - 1)
      singleton.task_config.conf_params[CONF_FILE].value = argv[i + 1];
  }

  int thread_pool_size = std::thread::hardware_concurrency();
  QString api_path = "";

  if (!args.contains("-h", Qt::CaseInsensitive) && !args.contains("--help", Qt::CaseInsensitive))
  {
    QSettings s_config(singleton.task_config.conf_params[CONF_FILE].value.toString(), QSettings::IniFormat);

    thread_pool_size = s_config.value("frs/thread_pool_size", thread_pool_size).toInt();
    if (thread_pool_size <= 0 || thread_pool_size > std::thread::hardware_concurrency() * 4)
      thread_pool_size = std::thread::hardware_concurrency();
    api_path = s_config.value("frs/api_path", api_path).toString();
    singleton.logs_path = s_config.value("frs/logs_path", singleton.logs_path).toString();
    singleton.screenshot_path = s_config.value("frs/screenshot_path", singleton.screenshot_path).toString();
    singleton.http_server_screenthot_url = s_config.value("frs/http_server_screenthot_url", singleton.http_server_screenthot_url).toString();

    QString sql_driver = s_config.value("sql/driver", "QMYSQL").toString();
    QString sql_options = s_config.value("sql/options", "").toString();
    QString sql_host = s_config.value("sql/host", "localhost").toString();
    int sql_port = s_config.value("sql/port", 3306).toInt();
    QString sql_db_name = s_config.value("sql/db_name", "test_frs").toString();
    QString sql_user_name = s_config.value("sql/user_name", "user_frs").toString();
    QString sql_password = s_config.value("sql/password", "").toString();
    int sql_pool_size = s_config.value("sql/pool_size", 8).toInt();
    if (sql_pool_size <= 0)
      sql_pool_size = 8;

    singleton.sql_pool = make_unique<SQLPool>(sql_pool_size, "FRS_SQL_", sql_driver, sql_options,
      sql_host, sql_port, sql_db_name, sql_user_name, sql_password);

    SQLGuard sql_guard;
    QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());
    QSqlQuery q_tp(SQL_GET_CURRENT_TIME_POINT, sql_db);
    if (!q_tp.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_CURRENT_TIME_POINT.");
      singleton.addLog(q_tp.lastError().text());
      return 1;
    }
    q_tp.next();

    QSqlQuery q_config(SQL_GET_COMMON_SETTINGS, sql_db);
    if (!q_config.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_COMMON_SETTINGS.");
      singleton.addLog(q_config.lastError().text());
      return 1;
    }
    QHash<QString, QString> config;
    QHash<QString, int> config_to_time_point;
    config.setSharable(false);
    QSet<QString> global_conf_params;
    while (q_config.next())
    {
      config[q_config.value("param_name").toString()] = q_config.value("param_value").toString();
      config_to_time_point[q_config.value("param_name").toString()] = q_config.value("unix_time_point").toInt();
      if (q_config.value("for_vstream").toInt() == 0)
        global_conf_params.insert(q_config.value("param_name").toString());
    }

    if (!checkConfig(config))
      return 1;

    //для теста
    //singleton.task_config.conf_params[CONF_LOGS_LEVEL].value = LOGS_VERBOSE;

    singleton.face_width = singleton.task_config.conf_params[CONF_DNN_FR_INPUT_WIDTH].value.toInt();
    singleton.face_height = singleton.task_config.conf_params[CONF_DNN_FR_INPUT_HEIGHT].value.toInt();
    singleton.descriptor_size = singleton.task_config.conf_params[CONF_DNN_FR_OUTPUT_SIZE].value.toInt();

    singleton.task_config.vstream_conf_params[0].setSharable(false);
    singleton.task_config.vstream_conf_params[0] = singleton.task_config.conf_params;
    for (const auto& global_conf_param : global_conf_params)
      singleton.task_config.vstream_conf_params[0].remove(global_conf_param);
    singleton.task_config.vstream_conf_params[0].remove(CONF_FILE);

    singleton.id_worker = s_config.value("sql/id_worker", 0).toInt();
    QSqlQuery q_worker(SQL_GET_WORKER_BY_ID.arg(singleton.id_worker), sql_db);
    if (!q_worker.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_WORKER_BY_ID.");
      singleton.addLog(q_worker.lastError().text());
      return 1;
    }
    int id_worker = 0;
    if (q_worker.next())
      id_worker = q_worker.value("id_worker").toInt();
    if (id_worker != singleton.id_worker || singleton.id_worker == 0)
    {
      singleton.addLog("  Ошибка: неправильно указан в конфигурации параметр id_worker.");
      return 1;
    }

    //видеопотоки
    QSqlQuery q_vstreams(sql_db);
    if (!q_vstreams.prepare(SQL_GET_VIDEO_STREAMS))
    {
      singleton.addLog("  Ошибка: не удалось подготовить запрос SQL_GET_VIDEO_STREAMS.");
      singleton.addLog(q_vstreams.lastError().text());
      return 1;
    }
    q_vstreams.bindValue(":id_worker", singleton.id_worker);
    if (!q_vstreams.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_VIDEO_STREAMS.");
      singleton.addLog(q_vstreams.lastError().text());
      return 1;
    }

    while (q_vstreams.next())
    {
      int id_vstream = q_vstreams.value("id_vstream").toInt();
      QString url = q_vstreams.value("url").toString().trimmed();
      QString vstream_ext = q_vstreams.value("vstream_ext").toString().trimmed();
      if (vstream_ext.isEmpty())
        vstream_ext =QString::number(id_vstream);
      QString callback_url = q_vstreams.value("callback_url").toString().trimmed();
      singleton.task_config.conf_vstream_url[id_vstream] = url;
      singleton.task_config.conf_vstream_callback_url[id_vstream] = callback_url;
      singleton.task_config.conf_id_vstream_to_vstream_ext[id_vstream] = vstream_ext;
      singleton.task_config.conf_vstream_ext_to_id_vstream[vstream_ext] = id_vstream;

      if (!q_vstreams.value("region_x").isNull() && !q_vstreams.value("region_y").isNull()
          && !q_vstreams.value("region_width").isNull() && !q_vstreams.value("region_height").isNull()
          && q_vstreams.value("region_width").toInt() > 0 && q_vstreams.value("region_height").toInt() > 0)
      {
        singleton.task_config.conf_vstream_region[id_vstream].x = q_vstreams.value("region_x").toInt();
        singleton.task_config.conf_vstream_region[id_vstream].y = q_vstreams.value("region_y").toInt();
        singleton.task_config.conf_vstream_region[id_vstream].width = q_vstreams.value("region_width").toInt();
        singleton.task_config.conf_vstream_region[id_vstream].height = q_vstreams.value("region_height").toInt();
      }

      singleton.task_config.vstream_conf_params[id_vstream].setSharable(false);
      singleton.task_config.vstream_conf_params[id_vstream] = singleton.task_config.vstream_conf_params[0];
    }

    //переопределенные параметры видеопотоков
    QSqlQuery q_vstream_settings(sql_db);
    if (!q_vstream_settings.prepare(SQL_GET_VIDEO_STREAM_SETTINGS))
    {
      singleton.addLog("  Ошибка: не удалось подготовить запрос SQL_GET_VIDEO_STREAM_SETTINGS.");
      singleton.addLog(q_vstream_settings.lastError().text());
      return 1;
    }
    q_vstream_settings.bindValue(":id_worker", singleton.id_worker);
    if (!q_vstream_settings.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_VIDEO_STREAM_SETTINGS.");
      singleton.addLog(q_vstream_settings.lastError().text());
      return 1;
    }

    while (q_vstream_settings.next())
    {
      int id_vstream = q_vstream_settings.value("id_vstream").toInt();
      QString param_name = q_vstream_settings.value("param_name").toString();
      QString param_value = q_vstream_settings.value("param_value").toString();
      switch (singleton.task_config.conf_params[param_name].value.type())
      {
        case QVariant::Bool:
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = bool(param_value.toInt());
          break;
        case QVariant::Int:
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = param_value.toInt();
          break;
        case QVariant::Double:
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = param_value.toDouble();
          break;
        default:
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = param_value;
          break;
      }
    }

    //привязки дескрипторов к видеопотокам
    QSqlQuery q_links_descriptor_vstream(sql_db);
    if (!q_links_descriptor_vstream.prepare(SQL_GET_LINKS_DESCRIPTOR_VSTREAM))
    {
      singleton.addLog("  Ошибка: не удалось подготовить запрос SQL_GET_LINKS_DESCRIPTOR_VSTREAM.");
      singleton.addLog(q_links_descriptor_vstream.lastError().text());
      return 1;
    }
    q_links_descriptor_vstream.bindValue(":id_worker", singleton.id_worker);
    if (!q_links_descriptor_vstream.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_LINKS_DESCRIPTOR_VSTREAM.");
      singleton.addLog(q_links_descriptor_vstream.lastError().text());
      return 1;
    }
    while (q_links_descriptor_vstream.next())
    {
      int id_descriptor = q_links_descriptor_vstream.value("id_descriptor").toInt();
      int id_vstream = q_links_descriptor_vstream.value("id_vstream").toInt();
      singleton.id_vstream_to_id_descriptors[id_vstream].setSharable(false);
      singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
      singleton.id_descriptor_to_id_vstreams[id_descriptor].setSharable(false);
      singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
    }

  } else
  {
    cout << "  --frs-config                         Файл с конфигурацией.\n\n";
  }

  ApiResource api_resource;

  //Wt сервер
  try
  {
    Wt::WServer server(argv[0], "");
    try
    {
      server.setServerConfiguration(argc, argv, WTHTTP_CONFIGURATION);
      server.addResource(&api_resource, api_path.toStdString());
      server.logger().setFile(server.appRoot() + "/web.log");
      if (server.start())
      {
        QCoreApplication app(argc, argv);
        singleton.working_path = QString::fromStdString(server.appRoot());
        singleton.loadData();
        singleton.task_scheduler = make_unique<Scheduler>(thread_pool_size,
          singleton.task_config.conf_params[CONF_LOGS_LEVEL].value.toInt());

        //для теста
        //singleton.task_scheduler->printCounters();

        singleton.task_scheduler->addPermanentTasks();

        //для теста
        singleton.task_scheduler->printCounters();

#ifdef WT_THREADED
        int sig = Wt::WServer::waitForShutdown();
        singleton.is_working = false;
        if (singleton.task_config.conf_params[CONF_LOGS_LEVEL].value.toInt() >= LOGS_EVENTS)
          singleton.addLog(QString("Получен сигнал %1 закрытия программы.").arg(sig));
        singleton.task_scheduler.reset();
        singleton.task_scheduler = nullptr;
        singleton.sql_pool.reset();
        singleton.sql_pool = nullptr;
#endif  // WT_THREADED

        server.stop();

#ifdef WT_THREADED
#ifndef WT_WIN32
        if (sig == SIGHUP)
          // Mac OSX: _NSGetEnviron()
          Wt::WServer::restart(argc, argv, nullptr);
#endif // WT_WIN32
#endif // WT_THREADED
      }
      return 0;
    } catch (std::exception& e)
    {
      if (!QString(e.what()).simplified().isEmpty())
        cerr << "  Фатальная ошибка: " << e.what() << endl;
      return 1;
    }
  } catch (Wt::WServer::Exception& e)
  {
    cerr << "  Фатальная ошибка WServer: " << e.what() << endl;
    return 1;
  } catch (std::exception& e)
  {
    cerr << "  Фатальная ошибка: " << e.what() << endl;
    return 1;
  }
}
