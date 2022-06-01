#include "frs_api.h"
#include "tasks.h"
#include "absl/strings/escaping.h"

#include "crow/json.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


using namespace std;

namespace
{
  int jsonInteger(const crow::json::rvalue& v, const string& key)
  {
    if (!v.has(key))
      return {};

    if (v[key].t() == crow::json::type::Number || v[key].t() == crow::json::type::String)
      try
      {
        return static_cast<int>(v[key]);
      } catch(...)
      {
        //ничего не делаем
      }

    return {};
  }

  int jsonInteger(const crow::json::rvalue& v)
  {
    if (v.t() == crow::json::type::Number)
      return static_cast<int>(v);

    if (v.t() == crow::json::type::String)
      try
      {
        return static_cast<int>(v);
      } catch(...)
      {
        //ничего не делаем
      }

    return {};
  }

  String jsonString(const crow::json::rvalue& v, const string& key)
  {
    if (!v.has(key))
      return {};

    if (v[key].t() == crow::json::type::String || v[key].t() == crow::json::type::True || v[key].t() == crow::json::type::False
      || v[key].t() == crow::json::type::Number)
      return string(v[key]);
    else
      return {};
  }
}

ApiService::~ApiService()
{
  auto& singleton = Singleton::instance();
  singleton.is_working.store(false, std::memory_order_relaxed);

  singleton.runtime->timer_queue()->shutdown();
  singleton.runtime->background_executor()->shutdown();
  singleton.runtime->thread_pool_executor()->shutdown();

  singleton.runtime.reset();
  singleton.runtime = nullptr;

  singleton.sql_client.reset();
  singleton.sql_client = nullptr;

  singleton.addLog("Завершение работы FRS.");
}

bool ApiService::checkInputParam(const crow::json::rvalue& json, crow::response& response, const char* input_param)
{
  if (json.error())
  {
    crow::json::wvalue json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
    json_error[API::P_MESSAGE] = String(API::ERROR_REQUEST_STRUCTURE);
    response.code = code;
    response.body = json_error.dump();
    return false;
  }
  if (!json.has(input_param))
  {
    crow::json::wvalue json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
    json_error[API::P_MESSAGE] = absl::Substitute(API::ERROR_MISSING_PARAMETER, input_param);
    response.code = code;
    response.body = json_error.dump();
    return false;
  }
  if (json[input_param].t() == crow::json::type::Null)
  {
    crow::json::wvalue json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
    json_error[API::P_MESSAGE] = absl::Substitute(API::ERROR_NULL_PARAMETER, input_param);
    response.code = code;
    response.body = json_error.dump();
    return false;
  }
  bool is_empty = false;
  if (json[input_param].t() == crow::json::type::List)
    is_empty = (json[input_param].size() == 0);
  if (json[input_param].t() == crow::json::type::True || json[input_param].t() == crow::json::type::False
      || json[input_param].t() == crow::json::type::Number
      || json[input_param].t() == crow::json::type::String)
    is_empty = String(json[input_param]).empty();

  if (is_empty)
  {
    crow::json::wvalue json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
    json_error[API::P_MESSAGE] = absl::Substitute(API::ERROR_EMPTY_VALUE, input_param);
    response.code = code;
    response.body = json_error.dump();
    return false;
  }

  return true;
}

void ApiService::simpleResponse(int code, crow::response& response)
{
  simpleResponse(code, "", response);
}

void ApiService::simpleResponse(int code, const String& msg, crow::response& response)
{
  crow::json::wvalue json_response;
  json_response[API::P_CODE] = code;
  json_response[API::P_NAME] = API::RESPONSE_RESULT.at(code);
  json_response[API::P_MESSAGE] = msg.empty() ? API::RESPONSE_MESSAGE.at(code) : msg;
  response.code = code;
  response.body = json_response.dump();
}

void ApiService::handleRequest(const crow::request& request, crow::response& response, const String& api_method)
{
  Singleton& singleton = Singleton::instance();

  response.set_header(API::CONTENT_TYPE, API::MIME_TYPE);
  response.code = API::CODE_NO_CONTENT;

  auto api_function = absl::AsciiStrToLower(api_method);

  //проверка корректности структуры тела запроса (JSON)
  auto json = crow::json::load(request.body);
  if (!request.body.empty() && json.error())
  {
    crow::json::wvalue json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
    json_error[API::P_MESSAGE] = String(API::ERROR_REQUEST_STRUCTURE);
    response.code = code;
    response.body = json_error.dump();
    return;
  }

  //обработка запросов
  if (api_function == absl::AsciiStrToLower(API::ADD_STREAM))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    String url = jsonString(json, API::P_URL);
    String callback_url = jsonString(json, API::P_CALLBACK_URL);
    singleton.addLog(absl::Substitute(API::LOG_CALL_ADD_STREAM, API::ADD_STREAM, vstream_ext, url, callback_url));

    std::vector<int> face_ids;
    if (json.has(API::P_FACE_IDS) && json[API::P_FACE_IDS].t() != crow::json::type::Null && json[API::P_FACE_IDS].t() != crow::json::type::List)
    {
      crow::json::wvalue json_error;
      int code = API::CODE_ERROR;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
      json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_FACE_IDS);
      response.code = code;
      response.body = json_error.dump();
      return;
    }
    auto face_ids_list = json.has(API::P_FACE_IDS) ? json[API::P_FACE_IDS] : crow::json::rvalue(crow::json::type::List);
    if (face_ids_list.t() == crow::json::type::List)
      for (const auto& json_value : face_ids_list)
      {
        int id_descriptor = jsonInteger(json_value);
        if (id_descriptor > 0)
          face_ids.push_back(id_descriptor);
      }

    HashMap<String, String> params;
    if (json.has(API::P_PARAMS) && json[API::P_PARAMS].t() != crow::json::type::Null && json[API::P_PARAMS].t() != crow::json::type::List)
    {
      crow::json::wvalue json_error;
      int code = API::CODE_ERROR;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
      json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_PARAMS);
      response.code = code;
      response.body = json_error.dump();
      return;
    }

    auto params_list = json.has(API::P_PARAMS) ? json[API::P_PARAMS] : crow::json::rvalue(crow::json::type::List);
    bool is_ok2 = true;
    if (params_list.t() == crow::json::type::List)
    {
      for (const auto& json_value : params_list)
        if (json_value.t() == crow::json::type::Object)
        {
          auto param_name = jsonString(json_value, API::P_PARAM_NAME);
          auto param_value = jsonString(json_value, API::P_PARAM_VALUE);
          if (!param_name.empty())
            params[param_name] = param_value;
          else
          {
            is_ok2 = false;
            break;
          }
        } else
        {
          is_ok2 = false;
          break;
        }
    }

    if (!is_ok2)
    {
      crow::json::wvalue json_error;
      int code = API::CODE_ERROR;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
      json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_PARAMS);
      response.code = code;
      response.body = json_error.dump();
      return;
    }

    is_ok2 = addVStream(vstream_ext, url, callback_url, face_ids, params);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::MOTION_DETECTION))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_START))
      return;

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    bool is_start = (jsonString(json, API::P_START) == "t");
    singleton.addLog(absl::Substitute(API::LOG_CALL_MOTION_DETECTION, API::MOTION_DETECTION, vstream_ext, is_start));

    motionDetection(vstream_ext, is_start);
    response.code = API::CODE_NO_CONTENT;

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::DOOR_IS_OPEN))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    singleton.addLog(absl::Substitute(API::LOG_CALL_DOOR_IS_OPEN, API::DOOR_IS_OPEN, vstream_ext));

    motionDetection(vstream_ext, false, true);
    response.code = API::CODE_NO_CONTENT;

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::BEST_QUALITY))
  {
    if (!json.has(API::P_LOG_FACES_ID) && (!json.has(API::P_STREAM_ID) || !json.has(API::P_DATE)))
      return;

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    int id_vstream = 0;

    int id_log = 0;
    if (json.has(API::P_LOG_FACES_ID))
      id_log = jsonInteger(json, API::P_LOG_FACES_ID);

    DateTime log_date;
    bool is_valid = absl::ParseTime(API::DATETIME_FORMAT, jsonString(json, API::P_DATE), absl::LocalTimeZone(), &log_date, nullptr);
    if (id_log == 0 && !is_valid)
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_DATE), response);
      return;
    }

    String event_uuid = jsonString(json, API::P_EVENT_UUID);
    singleton.addLog(absl::Substitute(API::LOG_CALL_BEST_QUALITY, API::BEST_QUALITY, id_log, vstream_ext,
      jsonString(json, API::P_DATE), event_uuid));

    double interval_before = 0.0;
    double interval_after = 0.0;
    bool do_copy_event_data = false;

    if (id_log == 0)
    {
      ReadLocker lock(singleton.mtx_task_config);
      if (singleton.task_config.conf_vstream_ext_to_id_vstream.contains(vstream_ext))
        id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.at(vstream_ext);
      if (id_vstream == 0)
        return;

      interval_before = singleton.getConfigParamValue<double>(CONF_BEST_QUALITY_INTERVAL_BEFORE, id_vstream);
      interval_after = singleton.getConfigParamValue<double>(CONF_BEST_QUALITY_INTERVAL_AFTER, id_vstream);
      do_copy_event_data = singleton.getConfigParamValue<bool>(CONF_COPY_EVENT_DATA);
    } else
    {
      ReadLocker lock(singleton.mtx_task_config);
      do_copy_event_data = singleton.getConfigParamValue<bool>(CONF_COPY_EVENT_DATA);
    }

    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      String sql_query = (id_log == 0) ? SQL_GET_LOG_FACE_BEST_QUALITY : SQL_GET_LOG_FACE_BY_ID;
      auto statement = mysql_session.sql(sql_query);
      auto result = ((id_log == 0) ? statement
        .bind(id_vstream)
        .bind(absl::FormatTime(API::DATETIME_FORMAT, log_date, absl::LocalTimeZone()))
        .bind(interval_before)
        .bind(absl::FormatTime(API::DATETIME_FORMAT, log_date, absl::LocalTimeZone()))
        .bind(interval_after) : statement.bind(id_log)).execute();
      auto row = result.fetchOne();
      if (row)
      {
        String screenshot = singleton.http_server_screenshot_url + row[0].get<std::string>();
        int face_left = row[1];
        int face_top = row[2];
        int face_width = row[3];
        int face_height = row[4];
        absl::ParseTime(API::DATETIME_FORMAT_LOG_FACES, row[5].get<std::string>(), absl::LocalTimeZone(), &log_date, nullptr);

        crow::json::wvalue json_data;
        json_data[API::P_SCREENSHOT] = screenshot;
        json_data[API::P_FACE_LEFT] = face_left;
        json_data[API::P_FACE_TOP] = face_top;
        json_data[API::P_FACE_WIDTH] = face_width;
        json_data[API::P_FACE_HEIGHT] = face_height;

        int code = API::CODE_SUCCESS;
        crow::json::wvalue json_response{
          {API::P_CODE, code},
          {API::P_NAME, API::RESPONSE_RESULT.at(code)},
          {API::P_MESSAGE, API::MSG_DONE},
          {API::P_DATA, json_data}
        };
        response.code = code;
        response.body = json_response.dump();

        if (do_copy_event_data)
        {
          std::error_code ec;
          auto orig_screenshot = singleton.screenshot_path / row[0].get<std::string>();
          auto event_screenshot = singleton.events_path / row[0].get<std::string>();
          String event_json = event_screenshot.parent_path() / (event_screenshot.stem() += ".json");

          //копируем, если нужно, данные события
          if (!std::filesystem::exists(event_json))
          {
            //копируем скриншот
            if (event_uuid.empty())
            {
              std::filesystem::create_directories(event_screenshot.parent_path(), ec);
              std::filesystem::copy(orig_screenshot, event_screenshot, std::filesystem::copy_options::overwrite_existing, ec);
            }

            //копируем JSON-данные с добавлением URL кадра и времени события
            {
              String orig_json = orig_screenshot.parent_path() / (orig_screenshot.stem() += ".json");
              const auto f_size = std::filesystem::file_size(orig_json, ec);
              if (!ec && f_size > 0)
              {
                ifstream fr_json(orig_json, std::ios::in | std::ios::binary);
                String s_json(f_size, '\0');
                fr_json.read(s_json.data(), static_cast<streamsize>(f_size));
                crow::json::wvalue j_event(crow::json::load(s_json));
                if (event_uuid.empty())
                  j_event["event_url"] = singleton.http_server_events_url + row[0].get<std::string>();
                else
                  j_event["event_uuid"] = event_uuid;
                j_event["event_date"] = row[5].get<std::string>();

                std::filesystem::create_directories(event_screenshot.parent_path(), ec);
                ofstream fw_json(event_json, std::ios::out | std::ios::binary);
                fw_json << j_event.dump();
              }
            }

            //копируем данные дескрипторов события
            {
              String orig_data = orig_screenshot.parent_path() / (orig_screenshot.stem() += ".dat");
              const auto f_size = std::filesystem::file_size(orig_data, ec);
              if (!ec && f_size > 0)
              {
                ifstream fr_data(orig_data, std::ios::in | std::ios::binary);
                String s_data(f_size, '\0');
                fr_data.read(s_data.data(), static_cast<streamsize>(f_size));

                String event_data = singleton.events_path / (absl::FormatTime(API::DATE_FORMAT, log_date, absl::LocalTimeZone()) + ".dat");

                scoped_lock lock(singleton.mtx_append_event);
                std::filesystem::create_directories(singleton.events_path, ec);
                ofstream fw_data(event_data, std::ios::app);
                fw_data.write(s_data.data(), static_cast<streamsize>(f_size));
              }
            }
          }
        }
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, (id_log == 0) ? "SQL_GET_LOG_FACE_BEST_QUALITY" : "SQL_GET_LOG_FACE_BY_ID"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::REGISTER_FACE))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_URL))
      return;

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    String url = jsonString(json, API::P_URL);
    int face_left = jsonInteger(json, API::P_FACE_LEFT);
    int face_top = jsonInteger(json, API::P_FACE_TOP);
    int face_width = jsonInteger(json, API::P_FACE_WIDTH);
    int face_height = jsonInteger(json, API::P_FACE_HEIGHT);
    int id_vstream = 0;
    String comments;

    singleton.addLog(absl::Substitute(API::LOG_CALL_REGISTER_FACE, API::REGISTER_FACE, vstream_ext, url,
      face_left, face_top, face_width, face_height));

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);

      auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
      if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
        id_vstream = it->second;
      if (id_vstream > 0)
        comments = singleton.getConfigParamValue<String>(CONF_COMMENTS_URL_IMAGE_ERROR, id_vstream);
    }

    if (id_vstream == 0)
    {
      auto err_msg = absl::Substitute(API::ERROR_INVALID_VSTREAM, vstream_ext);
      singleton.addLog(err_msg);
      simpleResponse(API::CODE_ERROR, err_msg, response);
      return;
    }

    auto task_data = TaskData(id_vstream, TASK_REGISTER_DESCRIPTOR);
    task_data.frame_url = url;
    auto rd = std::make_shared<RegisterDescriptorResponse>();
    rd->comments = comments;
    rd->face_left = face_left;
    rd->face_top = face_top;
    rd->face_width = face_width;
    rd->face_height = face_height;

    processFrame(task_data, rd, nullptr).get();

    crow::json::wvalue json_response;
    int code = API::CODE_SUCCESS;
    if (rd->id_descriptor > 0)
    {
      std::vector<uchar> buff_;
      cv::imencode(".jpg", rd->face_image, buff_);
      String mime_type = "image/jpeg";
      crow::json::wvalue json_data;
      json_data[API::P_FACE_ID] = rd->id_descriptor;
      json_data[API::P_FACE_LEFT] = rd->face_left;
      json_data[API::P_FACE_TOP] = rd->face_top;
      json_data[API::P_FACE_WIDTH] = rd->face_width;
      json_data[API::P_FACE_HEIGHT] = rd->face_height;
      json_data[API::P_FACE_IMAGE] = absl::Substitute("data:$0;base64,$1", mime_type,
        absl::Base64Escape(String((const char *)(buff_.data()), buff_.size())));

      json_response = {
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, API::MSG_DONE},
        {API::P_DATA, json_data}
      };
    } else
    {
      code = API::CODE_ERROR;
      json_response = {
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, rd->comments}
      };
    }

    response.code = code;
    response.body = json_response.dump();

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::ADD_FACES) || api_function == absl::AsciiStrToLower(API::REMOVE_FACES))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_FACE_IDS))
      return;

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    std::vector<int> face_ids;
    auto face_ids_list = json[API::P_FACE_IDS];
    for (const auto& json_value : face_ids_list)
    {
      int id_descriptor = jsonInteger(json_value);
      if (id_descriptor > 0)
        face_ids.push_back(id_descriptor);
    }

    auto face_ids_list2 = absl::StrJoin(face_ids, ", ");
    singleton.addLog(absl::Substitute(API::LOG_CALL_ADD_OR_REMOVE_FACES,
      (api_function == absl::AsciiStrToLower(API::ADD_FACES)) ? API::ADD_FACES : API::REMOVE_FACES, vstream_ext, face_ids_list2));

    bool is_ok2 = true;
    if (api_function == absl::AsciiStrToLower(API::ADD_FACES))
      is_ok2 = addFaces(vstream_ext, face_ids);
    if (api_function == absl::AsciiStrToLower(API::REMOVE_FACES))
      is_ok2 = removeFaces(vstream_ext, face_ids);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::LIST_STREAMS))
  {
    singleton.addLog(absl::Substitute(API::LOG_CALL_SIMPLE_METHOD, API::LIST_STREAMS));

    crow::json::wvalue::list json_data;
    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto r_vstreams = mysql_session.sql(SQL_GET_VIDEO_STREAMS)
        .bind(singleton.id_worker)
        .execute();
      auto statement_link_descriptor_vstream = mysql_session.sql(SQL_GET_LINKS_DESCRIPTOR_VSTREAM_BY_ID);
      for (auto row_vstream : r_vstreams.fetchAll())
      {
        int id_vstream = row_vstream[0];
        auto vstream_ext = row_vstream[1].get<std::string>();
        if (!vstream_ext.starts_with(API::VIRTUAL_VS))
        {
          crow::json::wvalue data;
          data[API::P_STREAM_ID] = vstream_ext;

          auto r_link_descriptor_vstream = statement_link_descriptor_vstream
            .bind(singleton.id_worker)
            .bind(id_vstream)
            .execute();
          vector<int> faces;
          for (auto row_link_descriptor_vstream : r_link_descriptor_vstream)
          {
            int id_descriptor = row_link_descriptor_vstream[0];
            faces.push_back(id_descriptor);
          }
          if (!faces.empty())
            data[API::P_FACE_IDS] = faces;
          json_data.push_back(std::move(data));
        }
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      crow::json::wvalue json_response{
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, API::MSG_DONE},
        {API::P_DATA, json_data}
      };
      response.code = code;
      response.body = json_response.dump();
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::REMOVE_STREAM))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    auto vstream_ext = jsonString(json, API::P_STREAM_ID);
    singleton.addLog(absl::Substitute(API::LOG_CALL_REMOVE_STREAM, API::REMOVE_STREAM, vstream_ext));

    if (!removeVStream(vstream_ext))
      simpleResponse(API::CODE_SERVER_ERROR, response);

    return;
  }

  //для теста
  if (api_function == absl::AsciiStrToLower(API::TEST_IMAGE))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_URL))
      return;

    auto vstream_ext = jsonString(json, API::P_STREAM_ID);
    int id_vstream = 0;
    auto url = jsonString(json, API::P_URL);
    singleton.addLog(absl::Substitute(API::LOG_TEST_IMAGE, API::TEST_IMAGE, vstream_ext, url));

    //scope for lick mutex
    {
      ReadLocker lock(singleton.mtx_task_config);
      auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
      if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
        id_vstream = it->second;
      if (id_vstream == 0)
        return;
    }

    if (singleton.is_working.load(std::memory_order_relaxed))
    {
      auto task_data = TaskData(id_vstream, TASK_TEST);
      task_data.frame_url = url;
      singleton.runtime->thread_pool_executor()->submit(processFrame, task_data, nullptr, nullptr);
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::LIST_ALL_FACES))
  {
    singleton.addLog(absl::Substitute(API::LOG_CALL_SIMPLE_METHOD, API::LIST_ALL_FACES));

    crow::json::wvalue::list json_data;
    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_GET_ALL_FACE_DESCRIPTORS).execute();
      for (auto row : result.fetchAll())
      {
        int id_descriptor = row[0];
        json_data.push_back(id_descriptor);
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_GET_ALL_FACE_DESCRIPTORS"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      crow::json::wvalue json_response{
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_DATA, json_data}
      };
      response.code = code;
      response.body = json_response.dump();
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::DELETE_FACES))
  {
    if (!checkInputParam(json, response, API::P_FACE_IDS))
      return;

    std::vector<int> face_ids;
    auto face_ids_list = json[API::P_FACE_IDS];
    for (const auto& json_value : face_ids_list)
    {
      int id_descriptor = jsonInteger(json_value);
      if (id_descriptor > 0)
        face_ids.push_back(id_descriptor);
    }
    auto face_ids_list2 = absl::StrJoin(face_ids, ", ");
    singleton.addLog(absl::Substitute(API::LOG_CALL_DELETE_FACES, API::DELETE_FACES, face_ids_list2));
    bool is_ok2 = deleteFaces(face_ids);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::GET_EVENTS))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_DATE_START))
      return;

    if (!checkInputParam(json, response, API::P_DATE_END))
      return;

    DateTime date_start;
    auto is_valid = absl::ParseTime(API::DATETIME_FORMAT, jsonString(json, API::P_DATE_START), absl::LocalTimeZone(), &date_start, nullptr);
    if (!is_valid)
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_DATE_START), response);
      return;
    }

    DateTime date_end;
    is_valid = absl::ParseTime(API::DATETIME_FORMAT, jsonString(json, API::P_DATE_END), absl::LocalTimeZone(), &date_end, nullptr);
    if (!is_valid)
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_DATE_END), response);
      return;
    }

    auto vstream_ext = jsonString(json, API::P_STREAM_ID);
    int id_vstream = 0;

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);
      auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
      if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
        id_vstream = it->second;
      if (id_vstream == 0)
        return;
    }

    singleton.addLog(absl::Substitute(API::LOG_CALL_GET_EVENTS,API::GET_EVENTS, vstream_ext,
      jsonString(json, API::P_DATE_START), jsonString(json, API::P_DATE_END)));

    crow::json::wvalue::list json_data;
    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_GET_LOG_FACES_FROM_INTERVAL)
        .bind(id_vstream)
        .bind(absl::FormatTime(API::DATETIME_FORMAT, date_start, absl::LocalTimeZone()))
        .bind(absl::FormatTime(API::DATETIME_FORMAT, date_end, absl::LocalTimeZone()))
        .execute();
      for (auto row : result.fetchAll())
      {
        String log_date = row[0].get<std::string>();
        int id_descriptor = row[1].isNull() ? 0 : row[1].get<int>();
        double quality = row[2].get<double>();
        String screenshot = row[3].get<std::string>();
        int face_left = row[4];
        int face_top = row[5];
        int face_width = row[6];
        int face_height = row[7];

        crow::json::wvalue json_item;
        json_item[API::P_DATE] = log_date.c_str();
        if (id_descriptor > 0)
          json_item[API::P_FACE_ID] = id_descriptor;
        json_item[API::P_QUALITY] = static_cast<int>(quality);
        json_item[API::P_SCREENSHOT] = screenshot.c_str();
        json_item[API::P_FACE_LEFT] = face_left;
        json_item[API::P_FACE_TOP] = face_top;
        json_item[API::P_FACE_WIDTH] = face_width;
        json_item[API::P_FACE_HEIGHT] = face_height;

        json_data.push_back(std::move(json_item));
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_GET_LOG_FACES_FROM_INTERVAL"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      crow::json::wvalue json_response{
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, API::MSG_DONE},
        {API::P_DATA, json_data}
      };
      response.code = code;
      response.body = json_response.dump();
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::GET_SETTINGS))
  {
    singleton.addLog(absl::Substitute(API::LOG_CALL_SIMPLE_METHOD, API::GET_SETTINGS));

    std::vector<String> params;
    if (!request.body.empty())
    {
      if (json.has(API::P_PARAMS) && json[API::P_PARAMS].t() != crow::json::type::Null && json[API::P_PARAMS].t() != crow::json::type::List)
      {
        crow::json::wvalue json_error;
        int code = API::CODE_ERROR;
        json_error[API::P_CODE] = code;
        json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
        json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_PARAMS);
        response.code = code;
        response.body = json_error.dump();
        return;
      }
      auto params_list = json.has(API::P_PARAMS) ? json[API::P_PARAMS] : crow::json::rvalue(crow::json::type::List);
      if (params_list.t() == crow::json::type::List)
        for (const auto& json_value : params_list)
          if (json_value.t() == crow::json::type::String)
            params.push_back(json_value.s());
    }

    crow::json::wvalue::list json_data;

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);

      if (params.empty())
      {
        for (const auto& conf_param : singleton.task_config.conf_params)
        {
          crow::json::wvalue json_item;
          json_item[API::P_PARAM_NAME] = conf_param.first;
          if (!conf_param.second.comments.empty())
            json_item[API::P_PARAM_COMMENTS] = conf_param.second.comments;
          if (std::holds_alternative<bool>(conf_param.second.value))
          {
            json_item[API::P_PARAM_TYPE] = TYPE_BOOL;
            json_item[API::P_PARAM_VALUE] = std::get<bool>(conf_param.second.value);
          }
          if (std::holds_alternative<int>(conf_param.second.value))
          {
            json_item[API::P_PARAM_TYPE] = TYPE_INT;
            json_item[API::P_PARAM_VALUE] = std::get<int>(conf_param.second.value);
            if (conf_param.second.min_value.index() != 0)
              json_item[API::P_PARAM_MIN_VALUE] = std::get<int>(conf_param.second.min_value);
            if (conf_param.second.max_value.index() != 0)
              json_item[API::P_PARAM_MAX_VALUE] = std::get<int>(conf_param.second.max_value);
          }
          if (std::holds_alternative<double>(conf_param.second.value))
          {
            json_item[API::P_PARAM_TYPE] = TYPE_DOUBLE;
            json_item[API::P_PARAM_VALUE] = std::get<double>(conf_param.second.value);
            if (conf_param.second.min_value.index() != 0)
              json_item[API::P_PARAM_MIN_VALUE] = std::get<double>(conf_param.second.min_value);
            if (conf_param.second.max_value.index() != 0)
              json_item[API::P_PARAM_MAX_VALUE] = std::get<double>(conf_param.second.max_value);
          }
          if (std::holds_alternative<String>(conf_param.second.value))
          {
            json_item[API::P_PARAM_TYPE] = TYPE_STRING;
            json_item[API::P_PARAM_VALUE] = std::get<String>(conf_param.second.value);
          }

          json_data.push_back(std::move(json_item));
        }
      } else
      {
        for (const auto& param : params)
          if (singleton.task_config.conf_params.contains(param))
          {
            crow::json::wvalue json_item;
            json_item[API::P_PARAM_NAME] = param;
            if (!singleton.task_config.conf_params[param].comments.empty())
              json_item[API::P_PARAM_COMMENTS] = singleton.task_config.conf_params[param].comments;
            if (std::holds_alternative<bool>(singleton.task_config.conf_params[param].value))
            {
              json_item[API::P_PARAM_TYPE] = TYPE_BOOL;
              json_item[API::P_PARAM_VALUE] = std::get<bool>(singleton.task_config.conf_params[param].value);
            }
            if (std::holds_alternative<int>(singleton.task_config.conf_params[param].value))
            {
              json_item[API::P_PARAM_TYPE] = TYPE_INT;
              json_item[API::P_PARAM_VALUE] = std::get<int>(singleton.task_config.conf_params[param].value);
              if (singleton.task_config.conf_params[param].min_value.index() != 0)
                json_item[API::P_PARAM_MIN_VALUE] = std::get<int>(singleton.task_config.conf_params[param].min_value);
              if (singleton.task_config.conf_params[param].max_value.index() != 0)
                json_item[API::P_PARAM_MAX_VALUE] = std::get<int>(singleton.task_config.conf_params[param].max_value);
            }
            if (std::holds_alternative<double>(singleton.task_config.conf_params[param].value))
            {
              json_item[API::P_PARAM_TYPE] = TYPE_DOUBLE;
              json_item[API::P_PARAM_VALUE] = std::get<double>(singleton.task_config.conf_params[param].value);
              if (singleton.task_config.conf_params[param].min_value.index() != 0)
                json_item[API::P_PARAM_MIN_VALUE] = std::get<double>(singleton.task_config.conf_params[param].min_value);
              if (singleton.task_config.conf_params[param].max_value.index() != 0)
                json_item[API::P_PARAM_MAX_VALUE] = std::get<double>(singleton.task_config.conf_params[param].max_value);
            }
            if (std::holds_alternative<String>(singleton.task_config.conf_params[param].value))
            {
              json_item[API::P_PARAM_TYPE] = TYPE_STRING;
              json_item[API::P_PARAM_VALUE] = std::get<String>(singleton.task_config.conf_params[param].value);
            }

            json_data.push_back(std::move(json_item));
          }
      }
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      crow::json::wvalue json_response{
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, API::MSG_DONE},
        {API::P_DATA, json_data}
      };
      response.code = code;
      response.body = json_response.dump();
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::SET_SETTINGS))
  {
    if (!checkInputParam(json, response, API::P_PARAMS))
      return;

    singleton.addLog(absl::Substitute(API::LOG_CALL_SIMPLE_METHOD, API::SET_SETTINGS));

    HashMap<String, String> params;
    if (json[API::P_PARAMS].t() != crow::json::type::List)
    {
      crow::json::wvalue json_error;
      int code = API::CODE_ERROR;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
      json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_PARAMS);
      response.code = code;
      response.body = json_error.dump();
      return;
    }

    auto params_list = json[API::P_PARAMS];
    bool is_ok2 = true;
    for (const auto& json_value : params_list)
      if (json_value.t() == crow::json::type::Object)
      {
        auto param_name = jsonString(json_value, API::P_PARAM_NAME);
        auto param_value = jsonString(json_value, API::P_PARAM_VALUE);
        if (!param_name.empty())
          params[param_name] = param_value;
        else
        {
          is_ok2 = false;
          break;
        }
      } else
      {
        is_ok2 = false;
        break;
      }

    if (!is_ok2)
    {
      crow::json::wvalue json_error;
      int code = API::CODE_ERROR;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
      json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_PARAMS);
      response.code = code;
      response.body = json_error.dump();
      return;
    }

    is_ok2 = setParams(params);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::ADD_SPECIAL_GROUP))
  {
    if (!checkInputParam(json, response, API::P_SPECIAL_GROUP_NAME))
      return;

    String group_name = jsonString(json, API::P_SPECIAL_GROUP_NAME);
    auto max_descriptor_count = jsonInteger(json, API::P_MAX_DESCRIPTOR_COUNT);
    if (json.has(API::P_MAX_DESCRIPTOR_COUNT) && max_descriptor_count < TaskConfig::SG_MIN_DESCRIPTOR_COUNT)
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_MAX_DESCRIPTOR_COUNT), response);
      return;
    }
    if (max_descriptor_count <= 0)
      max_descriptor_count = TaskConfig::DEFAULT_SG_MAX_DESCRIPTOR_COUNT;

    singleton.addLog(absl::Substitute(API::LOG_CALL_ADD_SPECIAL_GROUP, API::ADD_SPECIAL_GROUP, group_name, max_descriptor_count));

    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_GET_SGROUP_BY_NAME)
        .bind(group_name)
        .execute();
      if (result.fetchOne())
      {
        int code = API::CODE_ERROR;
        crow::json::wvalue json_error{
          {API::P_CODE, code},
          {API::P_NAME, API::RESPONSE_RESULT.at(code)},
          {API::P_MESSAGE, absl::Substitute(API::ERROR_SGROUP_ALREADY_EXISTS, API::P_SPECIAL_GROUP_NAME)}
        };
        response.code = code;
        response.body = json_error.dump();
        return;
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_GET_SGROUP_BY_NAME"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    auto [id_sgroup, token] = addSpecialGroup(group_name, max_descriptor_count);
    if (id_sgroup == 0 || token.empty())
    {
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    int code = API::CODE_SUCCESS;
    crow::json::wvalue json_response{
      {API::P_CODE, code},
      {API::P_NAME, API::RESPONSE_RESULT.at(code)},
      {API::P_MESSAGE, API::MSG_DONE},
      {
        API::P_DATA,
        {
          {API::P_GROUP_ID, id_sgroup},
          {API::P_SG_API_TOKEN, token}
        }
      }
    };
    response.code = code;
    response.body = json_response.dump();

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::UPDATE_SPECIAL_GROUP))
  {
    if (!checkInputParam(json, response, API::P_GROUP_ID))
      return;

    int id_sgroup = jsonInteger(json, API::P_GROUP_ID);
    String group_name = jsonString(json, API::P_SPECIAL_GROUP_NAME);

    //если указано пустое название специальной группы, то возвращаем ответ с ошибкой
    if (json.has(API::P_SPECIAL_GROUP_NAME) && group_name.empty())
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_SPECIAL_GROUP_NAME), response);
      return;
    }

    auto max_descriptor_count = jsonInteger(json, API::P_MAX_DESCRIPTOR_COUNT);

    //если указано некорректное максимальное количество дескрипторов, то возвращаем ответ с ошибкой
    if (json.has(API::P_MAX_DESCRIPTOR_COUNT) && max_descriptor_count < TaskConfig::SG_MIN_DESCRIPTOR_COUNT)
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_MAX_DESCRIPTOR_COUNT), response);
      return;
    }

    bool sg_exists = false;
    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);

      if (max_descriptor_count <= 0)
      {
        auto it = singleton.task_config.conf_sgroup_max_descriptor_count.find(id_sgroup);
        if (it != singleton.task_config.conf_sgroup_max_descriptor_count.end())
          max_descriptor_count = it->second;
        else
          max_descriptor_count = TaskConfig::DEFAULT_SG_MAX_DESCRIPTOR_COUNT;
      }

      sg_exists = singleton.task_config.conf_sgroup_max_descriptor_count.contains(id_sgroup);
    }

    if (!sg_exists)
    {
      simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_GROUP_ID), response);
      return;
    }

    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      if (group_name.empty())
      {
        //не указано название специальной группы - берем значение из базы
        auto result = mysql_session.sql(SQL_GET_SGROUP_BY_ID)
          .bind(id_sgroup)
          .execute();
        auto row = result.fetchOne();
        if (row)
          group_name = row[1].get<std::string>();
        else
        {
          //в базе нет специальной группы с указанным в запросе идентификатором - возвращаем ответ с ошибкой
          simpleResponse(API::CODE_ERROR, absl::Substitute(API::INCORRECT_PARAMETER, API::P_GROUP_ID), response);
          return;
        }
      } else
      {
        //указано новое название специальной группы в запросе - проверяем, что название уникально
        auto result = mysql_session.sql(SQL_GET_SGROUP_BY_NAME)
          .bind(group_name)
          .execute();
        auto row = result.fetchOne();
        if (row)
        {
          int temp_id_sgroup = row[0].get<int>();
          if (id_sgroup != temp_id_sgroup)
          {
            //существует другая специальная группа с указанным названием - возвращаем ответ с ошибкой
            int code = API::CODE_ERROR;
            crow::json::wvalue json_error{
              {API::P_CODE, code},
              {API::P_NAME, API::RESPONSE_RESULT.at(code)},
              {API::P_MESSAGE, absl::Substitute(API::ERROR_SGROUP_ALREADY_EXISTS, API::P_SPECIAL_GROUP_NAME)}
            };
            response.code = code;
            response.body = json_error.dump();
            return;
          }
        }
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_GET_SGROUP_BY_NAME"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    singleton.addLog(absl::Substitute(API::LOG_CALL_UPDATE_SPECIAL_GROUP, API::UPDATE_SPECIAL_GROUP, id_sgroup,
      group_name, max_descriptor_count));

    bool is_ok2 = updateSpecialGroup(id_sgroup, group_name, max_descriptor_count);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::DELETE_SPECIAL_GROUP))
  {
    if (!checkInputParam(json, response, API::P_GROUP_ID))
      return;

    int id_sgroup = jsonInteger(json, API::P_GROUP_ID);
    singleton.addLog(absl::Substitute(API::LOG_CALL_SG_SIMPLE_METHOD, API::DELETE_SPECIAL_GROUP, id_sgroup));

    auto mysql_session = singleton.sql_client->getSession();
    try
    {
      mysql_session.startTransaction();

      mysql_session.sql(SQL_REMOVE_SG_FACE_DESCRIPTORS)
        .bind(id_sgroup)
        .execute();

      mysql_session.sql(SQL_DELETE_SPECIAL_GROUP)
        .bind(id_sgroup)
        .execute();

      mysql_session.commit();
    } catch (const mysqlx::Error& err)
    {
      mysql_session.rollback();
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    //scope for lock mutex
    {
      WriteLocker lock(singleton.mtx_task_config);

      //удаляем токен авторизации специальной группы
      auto it = std::find_if(singleton.task_config.conf_token_to_sgroup.begin(), singleton.task_config.conf_token_to_sgroup.end(),
        [id_sgroup](auto&& p)
        {
          return p.second == id_sgroup;
        });
      if (it != singleton.task_config.conf_token_to_sgroup.end())
        singleton.task_config.conf_token_to_sgroup.erase(it);

      //удаляем callback специальной группы
      singleton.task_config.conf_sgroup_callback_url.erase(id_sgroup);

      //удаляем максимальное количество дескрипторов в специальной группе
      singleton.task_config.conf_sgroup_max_descriptor_count.erase(id_sgroup);

      //удаляем дескрипторы специальной группы
      auto it_sgroup = singleton.id_sgroup_to_id_descriptors.find(id_sgroup);
      if (it_sgroup != singleton.id_sgroup_to_id_descriptors.end())
        for (const auto& id_descriptor : it_sgroup->second)
          singleton.id_descriptor_to_data.erase(id_descriptor);
      singleton.id_sgroup_to_id_descriptors.erase(id_sgroup);
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::PROCESS_FRAME))
  {
    if (!json.has(API::P_STREAM_ID) && !json.has(API::P_SG_ID))
      return;

    if (!checkInputParam(json, response, API::P_URL))
      return;

    String url = jsonString(json, API::P_URL);
    String image_base64;
    if (url.starts_with("data:"))
    {
      auto pos_comma = url.find(',');
      if (pos_comma != std::string::npos)
        if (url.find(";base64,") != std::string::npos)
        {
          image_base64 = url.substr(pos_comma + 1);
          url = url.substr(0, pos_comma);
        }

      if (image_base64.empty())
      {
        crow::json::wvalue json_error;
        int code = API::CODE_ERROR;
        json_error[API::P_CODE] = code;
        json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
        json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_URL);
        response.code = code;
        response.body = json_error.dump();
        return;
      }
    }

    String vstream_ext = jsonString(json, API::P_STREAM_ID);
    int id_vstream = 0;

    int id_sgroup = 0;
    if (json.has(API::P_SG_ID))
      id_sgroup = jsonInteger(json, API::P_SG_ID);

    if (!vstream_ext.empty())
    {
      ReadLocker lock(singleton.mtx_task_config);
      if (singleton.task_config.conf_vstream_ext_to_id_vstream.contains(vstream_ext))
        id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.at(vstream_ext);
    }

    if (id_vstream > 0 && id_sgroup > 0)
      return;

    auto s_url = image_base64.empty() ? url : "base64";
    singleton.addLog(absl::Substitute(API::LOG_CALL_PROCESS_FRAME, API::PROCESS_FRAME, vstream_ext, id_sgroup, s_url));

    auto task_data = TaskData(id_vstream, TASK_PROCESS_FRAME);
    task_data.id_sgroup = id_sgroup;
    task_data.is_base64 = !image_base64.empty();
    if (task_data.is_base64)
      task_data.frame_url = std::move(image_base64);
    else
      task_data.frame_url = std::move(url);
    auto process_result = std::make_shared<ProcessFrameResult>();
    processFrame(task_data, nullptr, process_result).get();
    if (!process_result->id_descriptors.empty())
    {
      crow::json::wvalue::list face_ids;
      for (auto id_descriptor : process_result->id_descriptors)
        face_ids.push_back(id_descriptor);
      int code = API::CODE_SUCCESS;
      crow::json::wvalue json_response{
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, API::MSG_DONE},
        {API::P_DATA, face_ids}
      };
      response.code = code;
      response.body = json_response.dump();
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::SAVE_DNN_STATS_DATA))
  {
    singleton.addLog(absl::Substitute(API::LOG_CALL_SIMPLE_METHOD, API::SAVE_DNN_STATS_DATA));
    singleton.saveDNNStatsData();

    return;
  }

  int code = API::CODE_ERROR;
  crow::json::wvalue json_error{
    {API::P_CODE, code},
    {API::P_NAME, API::RESPONSE_RESULT.at(code)},
    {API::P_MESSAGE, String(API::ERROR_UNKNOWN_API_METHOD)}
  };
  response.code = code;
  response.body = json_error.dump();
}

bool ApiService::addVStream(const String& vstream_ext, const String& url_new, const String& callback_url_new,
  const std::vector<int>& face_ids, const HashMap<String, String>& params)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  int id_vstream = 0;
  String url;
  String callback_url;
  int region_x = 0;
  int region_y = 0;
  int region_width = 0;
  int region_height = 0;
  HashMap<String, ConfParam> vstream_params;
  HashSet<int> id_vstream_descriptors;
  HashMap<int, FaceDescriptor> id_descriptor_to_data;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
    if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
      id_vstream = it->second;
    if (id_vstream > 0)
    {
      auto it_string = singleton.task_config.conf_vstream_url.find(id_vstream);
      if (it_string != singleton.task_config.conf_vstream_url.end())
        url = it_string->second;

      it_string = singleton.task_config.conf_vstream_callback_url.find(id_vstream);
      if (it_string != singleton.task_config.conf_vstream_callback_url.end())
        callback_url = it_string->second;
      auto it_rect = singleton.task_config.conf_vstream_region.find(id_vstream);
      if (it_rect != singleton.task_config.conf_vstream_region.end())
      {
        region_x = it_rect->second.x;
        region_y = it_rect->second.y;
        region_width = it_rect->second.width;
        region_height = it_rect->second.height;
      }
      if (singleton.id_vstream_to_id_descriptors.contains(id_vstream))
        id_vstream_descriptors = singleton.id_vstream_to_id_descriptors[id_vstream];
    }
    if (singleton.task_config.vstream_conf_params.contains(id_vstream))
      vstream_params = singleton.task_config.vstream_conf_params[id_vstream];
  }

  if (id_vstream == 0)
  {
    //проверяем, есть ли видео поток в базе
    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_GET_VIDEO_STREAM_BY_EXT)
        .bind(vstream_ext)
        .execute();
      auto row = result.fetchOne();
      if (row)
      {
        id_vstream = row[0];
        url = row[2].get<std::string>();
        callback_url = row[3].get<std::string>();
        region_x = row[4];
        region_y = row[5];
        region_width = row[6];
        region_height = row[7];
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC_IN_FUNCTION, "SQL_GET_VIDEO_STREAM_BY_EXT", __FUNCTION__));
      singleton.addLog(err.what());
      return false;
    }
  }

  if (!url_new.empty())
    url = url_new;

  if (!callback_url_new.empty())
    callback_url = callback_url_new;

  for (const auto& param : params)
  {
    absl::string_view param_name = param.first;
    absl::string_view param_value = param.second;

    if (param_name == "region-x")
    {
      int v;
      bool is_ok = absl::SimpleAtoi(param_value, &v);
      if (is_ok)
        region_x = v;
    }

    if (param_name == "region-y")
    {
      int v;
      bool is_ok = absl::SimpleAtoi(param_value, &v);
      if (is_ok)
        region_y = v;
    }

    if (param_name == "region-width")
    {
      int v;
      bool is_ok = absl::SimpleAtoi(param_value, &v);
      if (is_ok)
        region_width = v;
    }

    if (param_name == "region-height")
    {
      int v;
      bool is_ok = absl::SimpleAtoi(param_value, &v);
      if (is_ok)
        region_height = v;
    }

    if (vstream_params.contains(param_name))
    {
      bool is_ok = false;
      Variant v{};
      if (std::holds_alternative<bool>(vstream_params[param_name].value))
      {
        int q;
        is_ok = absl::SimpleAtoi(param_value, &q);
        if (is_ok)
          v = static_cast<bool>(q);
      }
      if (std::holds_alternative<int>(vstream_params[param_name].value))
      {
        int q;
        is_ok = absl::SimpleAtoi(param_value, &q);
        if (is_ok)
          v = q;
      }
      if (std::holds_alternative<double>(vstream_params[param_name].value))
      {
        double q;
        is_ok = absl::SimpleAtod(param_value, &q);
        if (is_ok)
          v = q;
      }
      if (std::holds_alternative<String>(vstream_params[param_name].value))
      {
        is_ok = true;
        v = param.second;
      }

      if (is_ok)
        vstream_params[param_name].value = v;
    }
  }

  auto mysql_session = singleton.sql_client->getSession();
  try
  {
    mysql_session.startTransaction();
    String sql_query = (id_vstream == 0) ? SQL_ADD_VSTREAM : SQL_UPDATE_VSTREAM;
    auto result = mysql_session.sql(sql_query)
      .bind(url)
      .bind(callback_url)
      .bind(region_x)
      .bind(region_y)
      .bind(region_width)
      .bind(region_height)
      .bind(id_vstream == 0 ? mysqlx::Value(vstream_ext) : mysqlx::Value(id_vstream))
      .execute();

    if (id_vstream == 0)
      id_vstream = static_cast<int>(result.getAutoIncrementValue());

    mysql_session.sql(SQL_ADD_LINK_WORKER_VSTREAM)
      .bind(singleton.id_worker)
      .bind(id_vstream)
      .bind(singleton.id_worker)
      .execute();

    //пишем конфигурационные параметры
    auto statement = mysql_session.sql(SQL_SET_VIDEO_STREAM_PARAM);
    for (auto& vstream_param : vstream_params)
    {
      //для теста
      //cout << String("__set vstream param %1, %2, %3\n").arg(id_vstream).arg(it.key(), it.value().value.toString()).toStdString();

      statement
        .bind(id_vstream)
        .bind(vstream_param.first)
        .bind(Singleton::variantToString(vstream_param.second.value))
        .bind(Singleton::variantToString(vstream_param.second.value))
        .execute();
    }

    //привязываем дескрипторы
    auto stmt_descriptor = mysql_session.sql(SQL_GET_FACE_DESCRIPTOR_BY_ID);
    auto stmt_link_descriptor_vstream = mysql_session.sql(SQL_ADD_LINK_DESCRIPTOR_VSTREAM);
    for (const auto& id_descriptor : face_ids)
    {
      //для теста
      //cout << "__descriptor " << id_descriptor;

      auto row_descriptor = stmt_descriptor.bind(id_descriptor).execute().fetchOne();
      if (row_descriptor)
        if (row_descriptor[0].get<int>() == id_descriptor && id_descriptor > 0)
        {
          //для теста
          //cout << " found.\n";

          if (!id_vstream_descriptors.contains(id_descriptor))
          {
            FaceDescriptor fd;
            fd.create(1, int(singleton.descriptor_size), CV_32F);
            auto s = row_descriptor[1].get<std::string >();
            std::memcpy(fd.data, s.data(), s.size());
            double norm_l2 = cv::norm(fd, cv::NORM_L2);
            if (norm_l2 <= 0.0)
              norm_l2 = 1.0;
            fd = fd / norm_l2;
            id_descriptor_to_data[id_descriptor] = std::move(fd);
          }

          stmt_link_descriptor_vstream
            .bind(id_descriptor)
            .bind(id_vstream)
            .execute();
        }
        //для теста
        /*else
          cout << " not found.\n";*/
    }

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    singleton.addLog(err.what());
    return false;
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);
    singleton.task_config.conf_id_vstream_to_vstream_ext[id_vstream] = vstream_ext;
    singleton.task_config.conf_vstream_ext_to_id_vstream[vstream_ext] = id_vstream;
    singleton.task_config.conf_vstream_url[id_vstream] = url;
    singleton.task_config.conf_vstream_callback_url[id_vstream] = callback_url;
    singleton.task_config.vstream_conf_params[id_vstream] = vstream_params;
    if (region_width > 0 && region_height > 0)
    {
      singleton.task_config.conf_vstream_region[id_vstream].x = region_x;
      singleton.task_config.conf_vstream_region[id_vstream].y = region_y;
      singleton.task_config.conf_vstream_region[id_vstream].width = region_width;
      singleton.task_config.conf_vstream_region[id_vstream].height = region_height;
    } else
      singleton.task_config.conf_vstream_region.erase(id_vstream);

    for (auto& it : id_descriptor_to_data)
    {
      int id_descriptor = it.first;

      //для теста
      //cout << "__new descriptor for stream: " << id_descriptor << endl;

      if (!singleton.id_descriptor_to_data.contains(id_descriptor))
      {
        singleton.id_descriptor_to_data[id_descriptor] = std::move(it.second);

        //для теста
        //cout << "__descriptor loaded from database.\n";
      }
      singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
      singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
    }

    singleton.tp_task_config = std::chrono::steady_clock::now();
  }

  return true;
}

void ApiService::motionDetection(const String& vstream_ext, bool is_start, bool is_door_open)
{
  if (is_door_open)
    is_start = false;
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
      return;

  int id_vstream = 0;
  double conf_open_door_duration = 0.0;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
    if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
    {
      id_vstream = it->second;
      conf_open_door_duration = singleton.getConfigParamValue<double>(CONF_OPEN_DOOR_DURATION, id_vstream);
    }
  }

  if (id_vstream == 0)
    return;

  bool do_task = false;
  bool do_check_motion_task = false;
  auto now = std::chrono::steady_clock::now();

  //scope for lock mutex
  {
    scoped_lock lock(singleton.mtx_capture);

    if (is_door_open)
    {
      if (!singleton.is_door_open.contains(id_vstream))
      {
        do_check_motion_task = true;
        singleton.is_door_open.insert(id_vstream);
        singleton.is_capturing.insert(id_vstream);
      }
    } else
    {
      if (is_start)
        singleton.id_vstream_to_start_motion[id_vstream] = now;
      else
        singleton.id_vstream_to_end_motion[id_vstream] = now;
      if (is_start && !singleton.is_capturing.contains(id_vstream))
      {
        do_task = true;
        singleton.is_capturing.insert(id_vstream);
      }
    }
  }

  if (do_task && singleton.is_working.load(std::memory_order_relaxed))
  {
    auto task_data = TaskData(id_vstream, TASK_RECOGNIZE);
    singleton.runtime->thread_pool_executor()->submit(processFrame, task_data, nullptr, nullptr);
    singleton.addLog(absl::Substitute(API::LOG_START_MOTION, task_data.id_vstream));
  }

  if (do_check_motion_task && singleton.is_working.load(std::memory_order_relaxed))
  {
    auto task_data = TaskData(id_vstream, TASK_CHECK_MOTION);
    task_data.delay_s = conf_open_door_duration;
    singleton.runtime->thread_pool_executor()->submit(checkMotion, task_data);
  }
}

bool ApiService::addFaces(const String& vstream_ext, const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  int id_vstream = 0;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
    if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
      id_vstream = it->second;
  }

  if (id_vstream == 0)
    return true;

  HashSet<int> id_descriptors;
  std::vector<int> id_new_descriptors;

  auto mysql_session = singleton.sql_client->getSession();
  try
  {
    mysql_session.startTransaction();
    auto stmt_link_descriptor_vstream = mysql_session.sql(SQL_ADD_LINK_DESCRIPTOR_VSTREAM);

    for (const auto& id_descriptor : face_ids)
    {
      auto result = stmt_link_descriptor_vstream
        .bind(id_descriptor)
        .bind(id_vstream)
        .execute();
      if (result.getAffectedItemsCount() > 0)
        id_descriptors.insert(id_descriptor);
    }

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    singleton.addLog(err.what());
    return false;
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);

    for (const auto& id_descriptor : id_descriptors)
      if (singleton.id_descriptor_to_data.contains(id_descriptor))
      {
        singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
        singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
      } else
        id_new_descriptors.push_back(id_descriptor);
  }

  if (!id_new_descriptors.empty())  //привязываем новые дескрипторы, которые есть в базе, но нет в оперативной памяти
  {
    HashMap<int, FaceDescriptor> id_descriptor_to_data;

    try
    {
      mysql_session.startTransaction();
      auto stmt_descriptor = mysql_session.sql(SQL_GET_FACE_DESCRIPTOR_BY_ID);
      auto stmt_link_descriptor_vstream = mysql_session.sql(SQL_ADD_LINK_DESCRIPTOR_VSTREAM);

      for (const auto& id_descriptor : id_new_descriptors)
      {
        auto row = stmt_descriptor.bind(id_descriptor).execute().fetchOne();
        if (row)
          if (row[0].get<int>() == id_descriptor && id_descriptor > 0)
          {
            //для теста
            //cout << "load from db descriptor: " << id_descriptor << endl;

            FaceDescriptor fd;
            fd.create(1, int(singleton.descriptor_size), CV_32F);
            std::string s = row[1].get<std::string>();
            std::memcpy(fd.data, s.data(), s.size());
            double norm_l2 = cv::norm(fd, cv::NORM_L2);
            if (norm_l2 <= 0.0)
              norm_l2 = 1.0;
            fd = fd / norm_l2;
            id_descriptor_to_data[id_descriptor] = std::move(fd);

            stmt_link_descriptor_vstream
              .bind(id_descriptor)
              .bind(id_vstream)
              .execute();
          }
      }

      mysql_session.commit();
    } catch (const mysqlx::Error& err)
    {
      mysql_session.rollback();
      singleton.addLog(err.what());
      return false;
    }

    if (!id_descriptor_to_data.empty())
    {
      WriteLocker lock(singleton.mtx_task_config);
      for (auto & it : id_descriptor_to_data)
      {
        int id_descriptor = it.first;
        singleton.id_descriptor_to_data[id_descriptor] = std::move(it.second);
        singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
        singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
      }

      singleton.tp_task_config = std::chrono::steady_clock::now();
    }
  }

  return true;
}

bool ApiService::removeFaces(const String& vstream_ext, const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  int id_vstream = 0;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
    if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
      id_vstream = it->second;
  }

  if (id_vstream == 0)
    return true;

  HashSet<int> id_descriptors;
  HashSet<int> id_descriptors_remove;

  auto mysql_session = singleton.sql_client->getSession();
  try
  {
    mysql_session.startTransaction();
    auto stat_link_descriptor_vstream = mysql_session.sql(SQL_REMOVE_LINK_DESCRIPTOR_VSTREAM);
    auto stat_check_link = mysql_session.sql(SQL_CHECK_LINKS_DESCRIPTOR_VSTREAM);

    for (const auto& id_descriptor : face_ids)
    {
      auto result = stat_link_descriptor_vstream
        .bind(id_descriptor)
        .bind(id_vstream)
        .execute();
      if (result.getAffectedItemsCount() > 0)
      {
        id_descriptors.insert(id_descriptor);
        if (!stat_check_link
          .bind(singleton.id_worker)
          .bind(id_descriptor)
          .execute()
          .fetchOne())
          id_descriptors_remove.insert(id_descriptor);
      }
    }

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    singleton.addLog(err.what());
    return false;
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);

    for (const auto& id_descriptor : id_descriptors)
    {
      singleton.id_descriptor_to_id_vstreams[id_descriptor].erase(id_vstream);
      if (singleton.id_descriptor_to_id_vstreams[id_descriptor].empty())
        singleton.id_descriptor_to_id_vstreams.erase(id_descriptor);
      singleton.id_vstream_to_id_descriptors[id_vstream].erase(id_descriptor);
      if (id_descriptors_remove.contains(id_descriptor))
        singleton.id_descriptor_to_data.erase(id_descriptor);
    }
  }

  return true;
}

bool ApiService::deleteFaces(const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  auto mysql_session = singleton.sql_client->getSession();
  try
  {
    mysql_session.startTransaction();
    auto statement = mysql_session.sql(SQL_REMOVE_FACE_DESCRIPTOR);
    for (const auto& id_descriptor : face_ids)
      statement.bind(id_descriptor).execute();

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    singleton.addLog(err.what());
    return false;
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);

    for (const auto& id_descriptor : face_ids)
    {
      //удаляем привязку к видео потокам
      for (auto it = singleton.id_descriptor_to_id_vstreams[id_descriptor].cbegin(); it != singleton.id_descriptor_to_id_vstreams[id_descriptor].cend(); ++it)
        singleton.id_vstream_to_id_descriptors[*it].erase(id_descriptor);
      singleton.id_descriptor_to_id_vstreams.erase(id_descriptor);

      //удаляем дескриптор
      singleton.id_descriptor_to_data.erase(id_descriptor);
    }
  }

  return true;
}

bool ApiService::removeVStream(const String& vstream_ext)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  int id_vstream = 0;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    auto it = singleton.task_config.conf_vstream_ext_to_id_vstream.find(vstream_ext);
    if (it != singleton.task_config.conf_vstream_ext_to_id_vstream.end())
      id_vstream = it->second;
  }

  if (id_vstream == 0)
    return true;

  //для теста
  //cout << "__id_vstream: " << id_vstream << endl;

  HashSet<int> id_descriptors_remove;

  auto mysql_session = singleton.sql_client->getSession();
  try
  {
    mysql_session.startTransaction();
    auto result = mysql_session.sql(SQL_GET_SOLE_LINK_DESCRIPTORS_VSTREAM)
      .bind(id_vstream)
      .bind(id_vstream)
      .execute();
    for (auto row : result)
    {
      id_descriptors_remove.insert(row[0]);

      //для теста
      //cout << "__descriptor to remove: " << q_link_descriptor_vstream.value("id_descriptor").toInt() << endl;
    }

    mysql_session.sql(SQL_REMOVE_LINK_WORKER_VSTREAM)
      .bind(singleton.id_worker)
      .bind(id_vstream)
      .execute();

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    singleton.addLog(err.what());
    return false;
  }

  //scope for lock mutex
  {
    scoped_lock lock(singleton.mtx_capture);
    singleton.id_vstream_to_start_motion.erase(id_vstream);
    singleton.id_vstream_to_end_motion.erase(id_vstream);
    singleton.is_capturing.erase(id_vstream);
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);

    //удаляем привязки дескрипторов к видео потоку и ненужные дескрипторы
    singleton.id_vstream_to_id_descriptors.erase(id_vstream);
    for (const auto& id_descriptor : id_descriptors_remove)
      singleton.id_descriptor_to_data.erase(id_descriptor);

    //удаляем конфиги видео потока
    singleton.task_config.conf_id_vstream_to_vstream_ext.erase(id_vstream);
    singleton.task_config.conf_vstream_ext_to_id_vstream.erase(vstream_ext);
    singleton.task_config.conf_vstream_url.erase(id_vstream);
    singleton.task_config.conf_vstream_callback_url.erase(id_vstream);
    singleton.task_config.conf_vstream_region.erase(id_vstream);
    singleton.task_config.vstream_conf_params.erase(id_vstream);
  }

  return true;
}

std::tuple<int, String> ApiService::addSpecialGroup(const String& group_name, int max_descriptor_count)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return {};

  boost::uuids::random_generator gen;
  boost::uuids::uuid id = gen();
  String token = boost::uuids::to_string(id);
  int id_sgroup{};

  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    auto result = mysql_session.sql(SQL_ADD_SPECIAL_GROUP)
      .bind(group_name)
      .bind(token)
      .bind(max_descriptor_count)
      .execute();
    id_sgroup = static_cast<int>(result.getAutoIncrementValue());
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog(absl::Substitute(ERROR_SQL_EXEC_IN_FUNCTION, "SQL_ADD_SPECIAL_GROUP", __FUNCTION__));
    singleton.addLog(err.what());
    return {};
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);
    singleton.task_config.conf_token_to_sgroup[token] = id_sgroup;
    singleton.task_config.conf_sgroup_max_descriptor_count[id_sgroup] = max_descriptor_count;
  }

  return {id_sgroup, token};
}

bool ApiService::updateSpecialGroup(int id_sgroup, const String& group_name, int max_descriptor_count)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return {};

  try
  {
    auto mysql_session = singleton.sql_client->getSession();

    auto result = mysql_session.sql(SQL_UPDATE_SPECIAL_GROUP)
      .bind(group_name)
      .bind(max_descriptor_count)
      .bind(id_sgroup)
      .execute();
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog(absl::Substitute(ERROR_SQL_EXEC_IN_FUNCTION, "SQL_UPDATE_SPECIAL_GROUP", __FUNCTION__));
    singleton.addLog(err.what());
    return {};
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);
    singleton.task_config.conf_sgroup_max_descriptor_count[id_sgroup] = max_descriptor_count;
  }

  return true;
}

void ApiService::handleSGroupRequest(const crow::request& request, crow::response& response, const String& api_method_sgroup)
{
  Singleton& singleton = Singleton::instance();

  //проверяем авторизацию
  auto auth = request.get_header_value(String(API::AUTH_HEADER));
  if (!auth.starts_with(API::AUTH_TYPE))
  {
    simpleResponse(API::CODE_UNAUTHORIZED, response);
    return;
  }

  auto token = absl::ClippedSubstr(auth, API::AUTH_TYPE.size());
  int id_sgroup{};
  String comments;
  int conf_max_descriptor_count = TaskConfig::DEFAULT_SG_MAX_DESCRIPTOR_COUNT;
  int current_descriptor_count{};

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);

    auto it = singleton.task_config.conf_token_to_sgroup.find(token);
    if (it != singleton.task_config.conf_token_to_sgroup.end())
    {
      id_sgroup = it->second;
      comments = singleton.getConfigParamValue<String>(CONF_COMMENTS_URL_IMAGE_ERROR);
    }

    auto it2 = singleton.task_config.conf_sgroup_max_descriptor_count.find(id_sgroup);
    if (it2 != singleton.task_config.conf_sgroup_max_descriptor_count.end())
      conf_max_descriptor_count = it2->second;

    auto it3 = singleton.id_sgroup_to_id_descriptors.find(id_sgroup);
    if (it3 != singleton.id_sgroup_to_id_descriptors.end())
      current_descriptor_count = static_cast<int>(it3->second.size());
  }

  if (id_sgroup == 0)
  {
    simpleResponse(API::CODE_UNAUTHORIZED, response);
    return;
  }

  auto api_function = absl::AsciiStrToLower(api_method_sgroup);

  //проверка корректности структуры тела запроса (JSON)
  auto json = crow::json::load(request.body);
  if (!request.body.empty() && json.error())
  {
    crow::json::wvalue json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
    json_error[API::P_MESSAGE] = String(API::ERROR_REQUEST_STRUCTURE);
    response.code = code;
    response.body = json_error.dump();
    return;
  }

  //обработка запросов
  response.set_header(API::CONTENT_TYPE, API::MIME_TYPE);
  response.code = API::CODE_NO_CONTENT;

  if (api_function == absl::AsciiStrToLower(API::SG_REGISTER_FACE))
  {
    if (current_descriptor_count >= conf_max_descriptor_count)
    {
      simpleResponse(API::CODE_FORBIDDEN, absl::Substitute(API::ERROR_SGROUP_MAX_DESCRIPTOR_COUNT_LIMIT, conf_max_descriptor_count),
        response);
      return;
    }

    if (!checkInputParam(json, response, API::P_URL))
      return;

    String url = jsonString(json, API::P_URL);
    String image_base64;
    if (url.starts_with("data:"))
    {
      auto pos_comma = url.find(',');
      if (pos_comma != std::string::npos)
        if (url.find(";base64,") != std::string::npos)
        {
          image_base64 = url.substr(pos_comma + 1);
          url = url.substr(0, pos_comma);
        }

      if (image_base64.empty())
      {
        crow::json::wvalue json_error;
        int code = API::CODE_ERROR;
        json_error[API::P_CODE] = code;
        json_error[API::P_NAME] = API::RESPONSE_RESULT.at(code);
        json_error[API::P_MESSAGE] = absl::Substitute(API::INCORRECT_PARAMETER, API::P_URL);
        response.code = code;
        response.body = json_error.dump();
        return;
      }
    }

    int face_left = jsonInteger(json, API::P_FACE_LEFT);
    int face_top = jsonInteger(json, API::P_FACE_TOP);
    int face_width = jsonInteger(json, API::P_FACE_WIDTH);
    int face_height = jsonInteger(json, API::P_FACE_HEIGHT);

    auto s_url = image_base64.empty() ? url : "base64";
    singleton.addLog(absl::Substitute(API::LOG_CALL_SG_REGISTER_FACE, API::SG_REGISTER_FACE, id_sgroup, s_url,
      face_left, face_top, face_width, face_height));

    auto task_data = TaskData(0, TASK_REGISTER_DESCRIPTOR);
    task_data.id_sgroup = id_sgroup;
    task_data.is_base64 = !image_base64.empty();
    if (task_data.is_base64)
      task_data.frame_url = std::move(image_base64);
    else
      task_data.frame_url = std::move(url);
    auto rd = std::make_shared<RegisterDescriptorResponse>();
    rd->comments = std::move(comments);
    rd->face_left = face_left;
    rd->face_top = face_top;
    rd->face_width = face_width;
    rd->face_height = face_height;

    processFrame(task_data, rd, nullptr).get();

    crow::json::wvalue json_response;
    int code = API::CODE_SUCCESS;
    if (rd->id_descriptor > 0)
    {
      std::vector<uchar> buff_;
      cv::imencode(".jpg", rd->face_image, buff_);
      String mime_type = "image/jpeg";
      crow::json::wvalue json_data;
      json_data[API::P_FACE_ID] = rd->id_descriptor;
      json_data[API::P_FACE_LEFT] = rd->face_left;
      json_data[API::P_FACE_TOP] = rd->face_top;
      json_data[API::P_FACE_WIDTH] = rd->face_width;
      json_data[API::P_FACE_HEIGHT] = rd->face_height;
      json_data[API::P_FACE_IMAGE] = absl::Substitute("data:$0;base64,$1", mime_type,
        absl::Base64Escape(String((const char *)(buff_.data()), buff_.size())));

      json_response = {
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, API::MSG_DONE},
        {API::P_DATA, json_data}
      };
    } else
    {
      code = API::CODE_ERROR;
      json_response = {
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_MESSAGE, rd->comments}
      };
    }

    response.code = code;
    response.body = json_response.dump();

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::SG_DELETE_FACES))
  {
    if (!checkInputParam(json, response, API::P_FACE_IDS))
      return;

    std::vector<int> face_ids;
    auto face_ids_list = json[API::P_FACE_IDS];
    for (const auto& json_value : face_ids_list)
    {
      int id_descriptor = jsonInteger(json_value);
      if (id_descriptor > 0)
        face_ids.push_back(id_descriptor);
    }
    auto face_ids_list2 = absl::StrJoin(face_ids, ", ");
    singleton.addLog(absl::Substitute(API::LOG_CALL_SG_DELETE_FACES, API::DELETE_FACES, id_sgroup, face_ids_list2));
    bool is_ok2 = sgDeleteFaces(id_sgroup, face_ids);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::SG_LIST_FACES))
  {
    singleton.addLog(absl::Substitute(API::LOG_CALL_SG_SIMPLE_METHOD, API::SG_LIST_FACES, id_sgroup));

    crow::json::wvalue::list json_data;
    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_GET_SG_FACE_DESCRIPTORS)
        .bind(id_sgroup)
        .execute();
      for (auto row : result.fetchAll())
      {
        int id_descriptor = row[0];
        String face_image = row[1].get<std::string>();
        json_data.push_back({
          {API::P_FACE_ID, id_descriptor},
          {API::P_FACE_IMAGE, std::move(face_image)}
        });
      }
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_GET_SG_FACE_DESCRIPTORS"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      crow::json::wvalue json_response{
        {API::P_CODE, code},
        {API::P_NAME, API::RESPONSE_RESULT.at(code)},
        {API::P_DATA, json_data}
      };
      response.code = code;
      response.body = json_response.dump();
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::SG_UPDATE_GROUP))
  {
    if (!checkInputParam(json, response, API::P_CALLBACK_URL))
      return;

    String callback_url = jsonString(json, API::P_CALLBACK_URL);
    singleton.addLog(absl::Substitute(API::LOG_CALL_SG_UPDATE_GROUP, API::SG_UPDATE_GROUP, id_sgroup, callback_url));

    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_UPDATE_SGROUP_CALLBACK)
        .bind(callback_url)
        .bind(id_sgroup)
        .execute();
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_UPDATE_SGROUP_CALLBACK"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    //scope for lock mutex
    {
      WriteLocker lock(singleton.mtx_task_config);
      singleton.task_config.conf_sgroup_callback_url[id_sgroup] = callback_url;
    }

    return;
  }

  if (api_function == absl::AsciiStrToLower(API::SG_RENEW_TOKEN))
  {
    singleton.addLog(absl::Substitute(API::LOG_CALL_SG_SIMPLE_METHOD, API::SG_RENEW_TOKEN, id_sgroup));

    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    String new_token = boost::uuids::to_string(id);

    try
    {
      auto mysql_session = singleton.sql_client->getSession();
      auto result = mysql_session.sql(SQL_UPDATE_SGROUP_TOKEN)
        .bind(new_token)
        .bind(id_sgroup)
        .execute();
    } catch (const mysqlx::Error& err)
    {
      singleton.addLog(absl::Substitute(ERROR_SQL_EXEC, "SQL_UPDATE_SGROUP_TOKEN"));
      singleton.addLog(err.what());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    //scope for lock mutex
    {
      WriteLocker lock(singleton.mtx_task_config);

      //удаляем токен авторизации специальной группы
      auto it = std::find_if(singleton.task_config.conf_token_to_sgroup.begin(), singleton.task_config.conf_token_to_sgroup.end(),
        [id_sgroup](auto&& p)
        {
          return p.second == id_sgroup;
        });
      if (it != singleton.task_config.conf_token_to_sgroup.end())
        singleton.task_config.conf_token_to_sgroup.erase(it);

      //добавляем новый токен авторизации специальной группы
      singleton.task_config.conf_token_to_sgroup[new_token] = id_sgroup;
    }

    int code = API::CODE_SUCCESS;
    crow::json::wvalue json_response{
      {API::P_CODE, code},
      {API::P_NAME, API::RESPONSE_RESULT.at(code)},
      {API::P_MESSAGE, API::MSG_DONE},
      {
        API::P_DATA,
        {
          {API::P_SG_API_TOKEN, new_token}
        }
      }
    };

    response.code = code;
    response.body = json_response.dump();

    return;
  }

  int code = API::CODE_ERROR;
  crow::json::wvalue json_error{
    {API::P_CODE, code},
    {API::P_NAME, API::RESPONSE_RESULT.at(code)},
    {API::P_MESSAGE, String(API::ERROR_UNKNOWN_API_METHOD)}
  };
  response.code = code;
  response.body = json_error.dump();
}

bool ApiService::sgDeleteFaces(int id_sgroup, const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  vector<int> id_descriptors_remove;
  auto mysql_session = singleton.sql_client->getSession();
  try
  {
    mysql_session.startTransaction();
    auto statement = mysql_session.sql(SQL_REMOVE_SG_FACE_DESCRIPTOR);
    for (const auto& id_descriptor : face_ids)
      if (statement
      .bind(id_descriptor)
      .bind(id_sgroup)
      .execute().getAffectedItemsCount() > 0)
        id_descriptors_remove.emplace_back(id_descriptor);

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    singleton.addLog(err.what());
    return false;
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);

    auto it_sgroup = singleton.id_sgroup_to_id_descriptors.find(id_sgroup);
    if (it_sgroup != singleton.id_sgroup_to_id_descriptors.end())
      for (const auto& id_descriptor : id_descriptors_remove)
      {
        //удаляем привязку к специальной группе
        it_sgroup->second.erase(id_descriptor);

        //удаляем дескриптор
        singleton.id_descriptor_to_data.erase(id_descriptor);
      }
  }

  return true;
}

bool ApiService::setParams(const HashMap<String, String>& params)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working.load(std::memory_order_relaxed))
    return false;

  HashMap<String, ConfParam> conf_params;
  std::vector<String> updated_params;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    conf_params = singleton.task_config.conf_params;
  }

  for (const auto& param : params)
  {
    absl::string_view param_name = param.first;
    absl::string_view param_value = param.second;

    if (conf_params.contains(param_name))
    {
      bool is_ok = false;
      Variant v{};
      if (std::holds_alternative<bool>(conf_params[param_name].value))
      {
        int q;
        is_ok = absl::SimpleAtoi(param_value, &q);
        if (is_ok)
          v = static_cast<bool>(q);
      }
      if (std::holds_alternative<int>(conf_params[param_name].value))
      {
        int q;
        is_ok = absl::SimpleAtoi(param_value, &q);
        if (is_ok)
          v = q;
      }
      if (std::holds_alternative<double>(conf_params[param_name].value))
      {
        double q;
        is_ok = absl::SimpleAtod(param_value, &q);
        if (is_ok)
          v = q;
      }
      if (std::holds_alternative<String>(conf_params[param_name].value))
      {
        is_ok = true;
        v = param.second;
      }

      if (is_ok)
      {
        conf_params[param_name].value = v;
        updated_params.push_back(param.first);
      }
    }
  }

  if (!updated_params.empty())
  {
    auto mysql_session = singleton.sql_client->getSession();
    try
    {
      mysql_session.startTransaction();
      auto statement = mysql_session.sql(SQL_SET_COMMON_PARAM);
      for (const auto& param : updated_params)
      {
        statement
          .bind(Singleton::variantToString(conf_params[param].value))
          .bind(param)
          .execute();
      }
      mysql_session.commit();
    } catch (const mysqlx::Error& err)
    {
      mysql_session.rollback();
      singleton.addLog(err.what());
      return false;
    }
  }

  //scope for lock mutex
  {
    WriteLocker lock(singleton.mtx_task_config);

    singleton.task_config.conf_params = conf_params;
    for (const auto& param : updated_params)
      if (singleton.task_config.vstream_conf_params[0].contains(param))
        singleton.task_config.vstream_conf_params[0][param].value = conf_params[param].value;

    singleton.tp_task_config = std::chrono::steady_clock::now();
  }

  return true;
}
