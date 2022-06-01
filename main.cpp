#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "frs_api.h"
#include "singleton.h"

using namespace std;

bool checkConfig(HashMap<String, String>& config)
{
  Singleton& singleton = Singleton::instance();

  bool is_config_ok = true;
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    const auto& k = it->first;
    if (singleton.task_config.conf_params.contains(k))
    {
      bool is_ok = false;

      if (std::holds_alternative<bool>(singleton.task_config.conf_params[k].value))
      {
        int q;
        is_ok = absl::SimpleAtoi(it->second, &q);
        if (is_ok)
          singleton.task_config.conf_params[k].value = static_cast<bool>(q);
      }

      if (std::holds_alternative<int>(singleton.task_config.conf_params[k].value))
      {
        int q;
        is_ok = absl::SimpleAtoi(it->second, &q);
        if (is_ok)
          singleton.task_config.conf_params[k].value = q;
      }

      if (std::holds_alternative<double>(singleton.task_config.conf_params[k].value))
      {
        double q;
        is_ok = absl::SimpleAtod(it->second, &q);
        if (is_ok)
          singleton.task_config.conf_params[k].value = q;
      }

      if (std::holds_alternative<String>(singleton.task_config.conf_params[k].value))
      {
        is_ok = true;
        singleton.task_config.conf_params[k].value = config[k];
      }

      //проверка на диапазон значений
      if (is_ok)
      {
        bool is_ok_range = true;
        String s_range;
        auto d1 = singleton.task_config.conf_params[k].min_value;
        auto d2 = singleton.task_config.conf_params[k].max_value;
        if (d1.index() != 0 && d2.index() != 0)
        {
          is_ok_range = (singleton.task_config.conf_params[k].value >= d1 && singleton.task_config.conf_params[k].value <= d2);
          s_range = absl::Substitute("в диапазоне [$0; $1]", Singleton::variantToString(d1), Singleton::variantToString(d2));
        }
        if (d1.index() != 0 && d2.index() == 0)
        {
          is_ok_range = (singleton.task_config.conf_params[k].value >= d1);
          s_range = absl::Substitute("больше или равно $0", Singleton::variantToString(d1));
        }
        if (d1.index() == 0 && d2.index() != 0)
        {
          is_ok_range = (singleton.task_config.conf_params[k].value <= d2);
          s_range = absl::Substitute("меньше или равно $0", Singleton::variantToString(d2));
        }

        if (!is_ok_range)
        {
          cerr << absl::Substitute("Значение параметра $0 (указано \"$1\") должно быть $2.\n", k,  it->second, s_range);
          is_config_ok = false;
        }
      }

      if (!is_ok)
      {
        cerr << absl::Substitute("Неправильно указано значение параметра $0: $1\n", k, it->second);
        is_config_ok = false;
      }
    } else
    {
      cerr << absl::Substitute("Предупреждение: в конфигурации указан неизвестный параметр $0\n", k);
    }
  }

  return is_config_ok;
}

int main(int argc, char** argv)
{
  Singleton& singleton = Singleton::instance();

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
    , {CONF_RETRY_PAUSE, {30, "Пауза в секундах между попытками установки связи с видео потоком", 3, 10 * 60 *60}}
    , {CONF_FACE_ENLARGE_SCALE, {1.4, "Коэффициент расширения прямоугольной области лица для хранения", 1.0, 2.0}}
    , {CONF_ALPHA, {1.0, "Альфа-коррекция", 0.0, 1.0}}
    , {CONF_BETA, {0.0, "Бета-коррекция", 0.0, 255.0}}
    , {CONF_GAMMA, {1.0, "Гамма-коррекция", 0.1, 100.0}}
    , {CONF_CALLBACK_TIMEOUT, {2.0, "время ожидания ответа при вызове callback URL в секундах", 0.1, 5.0}}
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
    , {CONF_OPEN_DOOR_DURATION, {5.0, "Время открытия двери в секундах", 1.0, 30.0}}

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

    //для копирования данных события в отдельную директорию
    , {CONF_COPY_EVENT_DATA, {false}}
  };

  vector<String> args;
  for (int i = 0; i < argc; ++i)
  {
    args.emplace_back(argv[i]);
    if (String(argv[i]) == "--frs-config" && i < argc - 1)
      singleton.task_config.conf_params[CONF_FILE].value = argv[i + 1];
  }

  auto thread_pool_size = std::thread::hardware_concurrency();
  String api_path;
  int http_port = 9051;

  try
  {
    boost::property_tree::ptree s_config;
    boost::property_tree::ini_parser::read_ini(singleton.getConfigParamValue<String>(CONF_FILE), s_config);
    thread_pool_size = s_config.get<uint>("frs.thread_pool_size", thread_pool_size);
    if (thread_pool_size <= 0 || thread_pool_size > std::thread::hardware_concurrency() * 4)
      thread_pool_size = std::thread::hardware_concurrency();
    http_port = s_config.get<int>("frs.http_port", http_port);
    singleton.logs_path = s_config.get<String>("frs.logs_path", singleton.logs_path);
    singleton.screenshot_path = s_config.get<String>("frs.screenshot_path", singleton.screenshot_path);
    singleton.events_path = s_config.get<String>("frs.events_path", singleton.events_path);
    singleton.http_server_screenshot_url = s_config.get<String>("frs.http_server_screenshot_url", singleton.http_server_screenshot_url);
    singleton.http_server_events_url = s_config.get<String>("frs.http_server_events_url", singleton.http_server_events_url);

    String sql_host = s_config.get<String>("sql.host", "localhost");
    int sql_port = s_config.get<int>("sql.port", 3306);
    String sql_db_name = s_config.get<String>("sql.db_name", "test_frs");
    String sql_user_name = s_config.get<String>("sql.user_name", "user_frs");
    String sql_password = s_config.get<String>("sql.password", "");
    int sql_pool_size = s_config.get<int>("sql.pool_size", 8);
    if (sql_pool_size <= 0)
      sql_pool_size = static_cast<int>(std::thread::hardware_concurrency());
    String mysql_settings = sql_user_name + ":" + sql_password + "@" + sql_host + ":" + std::to_string(sql_port) + "/" + sql_db_name;
    singleton.sql_client = make_unique<mysqlx::Client>(mysql_settings, mysqlx::ClientOption::POOL_MAX_SIZE, sql_pool_size);
    singleton.id_worker = s_config.get<int>("sql.id_worker", 0);
  } catch (const exception& e)
  {
    cerr
      << absl::Substitute("Не удалось загрузить конфигурацию из файла $0\n", singleton.getConfigParamValue<String>(CONF_FILE))
      << e.what() << "\n"
      << "Используйте параметр --frs-config для указания файла с конфигурацией, например:\n/opt/frs/frs --frs-config /opt/frs/sample.config\n";
    return 1;
  }

  HashMap<String, String> config;
  HashMap<String, int> config_to_time_point;
  HashSet<String> global_conf_params;
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_COMMON_SETTINGS).execute();
    for (auto row : result.fetchAll())
    {
      if (row[1].isNull())
        continue;

      String param_name = row[0].get<std::string>();
      config[param_name] = row[1].get<std::string>();
      config_to_time_point[param_name] = row[5].get<int>();
      if (row[3].get<int>() == 0)
        global_conf_params.insert(param_name);
    }
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_COMMON_SETTINGS.");
    singleton.addLog(err.what());
    return 1;
  }

  if (!checkConfig(config))
    return 1;

  //для теста
  //singleton.task_config.conf_params[CONF_LOGS_LEVEL].value = LOGS_VERBOSE;

  singleton.face_width = singleton.getConfigParamValue<int>(CONF_DNN_FR_INPUT_WIDTH);
  singleton.face_height = singleton.getConfigParamValue<int>(CONF_DNN_FR_INPUT_HEIGHT);
  singleton.descriptor_size = singleton.getConfigParamValue<int>(CONF_DNN_FR_OUTPUT_SIZE);

  singleton.task_config.vstream_conf_params[0] = singleton.task_config.conf_params;
  for (const auto& global_conf_param : global_conf_params)
    singleton.task_config.vstream_conf_params[0].erase(global_conf_param);
  singleton.task_config.vstream_conf_params[0].erase(CONF_FILE);

  int id_worker = 0;
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_WORKER_BY_ID)
      .bind(singleton.id_worker)
      .execute();
    auto row = result.fetchOne();
    if (row)
      id_worker = row[0].get<int>();
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_WORKER_BY_ID.");
    singleton.addLog(err.what());
    return 1;
  }
  if (id_worker != singleton.id_worker || singleton.id_worker == 0)
  {
    singleton.addLog("  Ошибка: неправильно указан в конфигурации параметр id_worker.");
    return 1;
  }

  //видео потоки
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_VIDEO_STREAMS)
      .bind(singleton.id_worker)
      .execute();
    for (auto row : result.fetchAll())
    {
      int id_vstream = row[0].get<int>();
      String vstream_ext = row[1].get<std::string>();
      if (vstream_ext.empty())
        vstream_ext = std::to_string(id_vstream);
      String url = row[2].isNull() ? "" : row[2].get<std::string>();
      String callback_url = row[3].isNull() ? "" : row[3].get<std::string>();
      singleton.task_config.conf_vstream_url[id_vstream] = url;
      singleton.task_config.conf_vstream_callback_url[id_vstream] = callback_url;
      singleton.task_config.conf_id_vstream_to_vstream_ext[id_vstream] = vstream_ext;
      singleton.task_config.conf_vstream_ext_to_id_vstream[vstream_ext] = id_vstream;

      if (!row[4].isNull() && !row[5].isNull() && !row[6].isNull() && !row[7].isNull()
        && row[6].get<int>() > 0 && row[7].get<int>() > 0)
      {
        singleton.task_config.conf_vstream_region[id_vstream].x = row[4].get<int>();
        singleton.task_config.conf_vstream_region[id_vstream].y = row[5].get<int>();
        singleton.task_config.conf_vstream_region[id_vstream].width = row[6].get<int>();
        singleton.task_config.conf_vstream_region[id_vstream].height = row[7].get<int>();
      }

      singleton.task_config.vstream_conf_params[id_vstream] = singleton.task_config.vstream_conf_params[0];
    }
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_VIDEO_STREAMS.");
    singleton.addLog(err.what());
    return 1;
  }

  //переопределенные параметры видео потоков
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_VIDEO_STREAM_SETTINGS)
      .bind(singleton.id_worker)
      .execute();
    for (auto row : result.fetchAll())
    {
      if (row[2].isNull())
        continue;

      int id_vstream = row[0].get<int>();
      String param_name = row[1].get<std::string>();
      String param_value = row[2].get<std::string>();

      auto& v = singleton.task_config.conf_params[param_name].value;
      if (std::holds_alternative<bool>(v))
      {
        int q;
        if (absl::SimpleAtoi(param_value, &q))
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = static_cast<bool>(q);
      }
      if (std::holds_alternative<int>(v))
      {
        int q;
        if (absl::SimpleAtoi(param_value, &q))
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = q;
      }
      if (std::holds_alternative<double>(v))
      {
        double q;
        if (absl::SimpleAtod(param_value, &q))
          singleton.task_config.vstream_conf_params[id_vstream][param_name].value = static_cast<double>(q);
      }
      if (std::holds_alternative<String>(v))
        singleton.task_config.vstream_conf_params[id_vstream][param_name].value = param_value;
    }
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_VIDEO_STREAM_SETTINGS.");
    singleton.addLog(err.what());
    return 1;
  }

  //привязки дескрипторов к видео потокам
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_LINKS_DESCRIPTOR_VSTREAM)
      .bind(singleton.id_worker)
      .execute();
    for (auto row : result.fetchAll())
    {
      int id_descriptor = row[0].get<int>();
      int id_vstream = row[1].get<int>();
      singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
      singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
    }
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_LINKS_DESCRIPTOR_VSTREAM.");
    singleton.addLog(err.what());
    return 1;
  }

  //специальные группы
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_SPECIAL_GROUPS).execute();
    for (auto row : result.fetchAll())
    {
      int id_sgroup = row[0].get<int>();
      String group_name = row[1].get<std::string>();
      String token = row[2].get<std::string>();
      String callback_url = row[3].isNull() ? "" : row[3].get<std::string>();
      int max_descriptor_count = row[4].get<int>();
      singleton.task_config.conf_token_to_sgroup[token] = id_sgroup;
      singleton.task_config.conf_sgroup_callback_url[id_sgroup] = callback_url;
      singleton.task_config.conf_sgroup_max_descriptor_count[id_sgroup] = max_descriptor_count;
    }
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_SPECIAL_GROUPS.");
    singleton.addLog(err.what());
    return 1;
  }

  //привязки дескрипторов к специальным группам
  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_LINKS_DESCRIPTOR_SGROUP).execute();
    for (auto row : result.fetchAll())
    {
      int id_descriptor = row[0].get<int>();
      int id_sgroup = row[1].get<int>();
      singleton.id_sgroup_to_id_descriptors[id_sgroup].insert(id_descriptor);
    }
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog("  Ошибка при выполнении запроса SQL_GET_LINKS_DESCRIPTOR_SGROUP.");
    singleton.addLog(err.what());
    return 1;
  }

  singleton.addLog("Запуск FRS.");
  concurrencpp::runtime_options rt_options;
  rt_options.max_cpu_threads = thread_pool_size;
  rt_options.max_background_threads = concurrencpp::details::consts::k_background_threadpool_worker_count_factor * thread_pool_size;
  singleton.runtime = std::make_unique<concurrencpp::runtime>(rt_options);
  singleton.working_path = std::filesystem::path(argv[0]).parent_path();
  singleton.loadData();
  singleton.loadDNNStatsData();
  singleton.addPermanentTasks();

  ApiService api_service;
  CROW_ROUTE(api_service, "/api/testCallback").methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
    [](const crow::request& request)
    {
      //тестовый запрос для проверки callback
      Singleton::instance().addLog(absl::Substitute(API::LOG_CALL_SIMPLE_METHOD, API::TEST_CALLBACK));
      std::cout << request.body << "\n";
      return request.body;
    });

  CROW_ROUTE(api_service,  "/api/<path>").methods(crow::HTTPMethod::Post)(
    [](const crow::request& request, crow::response& response, const std::string& api_method)
    {
      ApiService::handleRequest(request, response, api_method);
      response.end();
    });

  CROW_ROUTE(api_service,  "/sgapi/<path>").methods(crow::HTTPMethod::Post)(
    [](const crow::request& request, crow::response& response, const std::string& api_method_sgroup)
    {
      ApiService::handleSGroupRequest(request, response, api_method_sgroup);
      response.end();
    });

  api_service.port(http_port).multithreaded().run();
}
