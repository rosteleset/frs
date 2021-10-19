#include "frs_api.h"
#include <Wt/WServer.h>

bool ApiResource::checkInputParam(const Wt::Json::Object& json, Wt::Http::Response& response,
  const char* input_param)
{
  if (json.get(input_param).isNull())
  {
    Wt::Json::Object json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT[code];
    json_error[API::P_MESSAGE] = qUtf8Printable(QString("Отсутствует параметр \"%1\" в запросе.").arg(input_param));
    response.setStatus(code);
    response.out() << Wt::Json::serialize(json_error, API::INDENT);
    return false;
  }
  bool is_empty = false;
  if (json.get(input_param).type() == Wt::Json::Type::Array)
    if (Wt::Json::Array(json.get(input_param)).empty())
      is_empty = true;
  if (json.get(input_param).type() == Wt::Json::Type::Bool
      || json.get(input_param).type() == Wt::Json::Type::Number
      || json.get(input_param).type() == Wt::Json::Type::String)
    is_empty = QString::fromStdString(json.get(input_param).toString()).trimmed().isEmpty();

  if (is_empty)
  {
    Wt::Json::Object json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT[code];
    json_error[API::P_MESSAGE] = qUtf8Printable(QString("Пустое значение у параметра \"%1\" в запросе.").arg(input_param));
    response.setStatus(code);
    response.out() << Wt::Json::serialize(json_error, API::INDENT);
    return false;
  }

  return true;
}

void ApiResource::simpleResponse(int code, Wt::Http::Response& response)
{
  simpleResponse(code, "", response);
}

void ApiResource::simpleResponse(int code, const QString& msg, Wt::Http::Response& response)
{
  Wt::Json::Object json_response;
  json_response[API::P_CODE] = code;
  json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
  json_response[API::P_MESSAGE] = msg.isEmpty() ? API::RESPONSE_MESSAGE[code] : qUtf8Printable(msg);
  response.setStatus(code);
  response.out() << Wt::Json::serialize(json_response, API::INDENT);
}

void ApiResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
  Singleton& singleton = Singleton::instance();
  if (singleton.task_scheduler == nullptr)
    return;

  response.setMimeType(API::MIME_TYPE);
  response.setStatus(API::CODE_NO_CONTENT);

  if (request.continuation() != nullptr)
  {
    if (request.continuation()->data().type() == typeid(std::shared_ptr<RegisterDescriptorResponse>))
    {
      auto rd = Wt::cpp17::any_cast<std::shared_ptr<RegisterDescriptorResponse>>(request.continuation()->data());
      Wt::Json::Object json_response;
      int code = API::CODE_SUCCESS;
      if (rd->id_descriptor > 0)
      {
        QByteArray face_data;
        std::vector<uchar> buff_;
        cv::imencode(".jpg", rd->face_image, buff_);
        face_data = QByteArray((char*)buff_.data(), buff_.size());
        QString mime_type = "image/jpeg";
        Wt::Json::Object json_data;
        json_data[API::P_FACE_ID] = rd->id_descriptor;
        json_data[API::P_FACE_LEFT] = rd->face_left;
        json_data[API::P_FACE_TOP] = rd->face_top;
        json_data[API::P_FACE_WIDTH] = rd->face_width;
        json_data[API::P_FACE_HEIGHT] = rd->face_height;
        json_data[API::P_FACE_IMAGE] = qUtf8Printable(QString("data:%1;base64,%2").arg(mime_type, face_data.toBase64().data()));

        json_response[API::P_CODE] = code;
        json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
        json_response[API::P_MESSAGE] = API::MSG_DONE;
        json_response[API::P_DATA] = json_data;
      } else
      {
        code = API::CODE_ERROR;
        json_response[API::P_CODE] = code;
        json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
        json_response[API::P_MESSAGE] = qUtf8Printable(rd->comments);
      }

      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_response, API::INDENT);
    }

    request.continuation()->setData(nullptr);

    return;
  }

  QString api_function = QString::fromStdString(request.pathInfo()).toLower();
  std::string body;
  char buffer[4096];
  while (request.in().read(buffer, sizeof(buffer)))
    body.append(buffer, sizeof(buffer));
  body.append(buffer, request.in().gcount());

  Wt::Json::Object json;
  Wt::Json::ParseError error;
  bool is_ok = body.empty() || Wt::Json::parse(body, json, error, true);
  if (!is_ok)
  {
    Wt::Json::Object json_error;
    int code = API::CODE_ERROR;
    json_error[API::P_CODE] = code;
    json_error[API::P_NAME] = API::RESPONSE_RESULT[code];
    json_error[API::P_MESSAGE] = "Ошибка в структуре запроса.";
    response.setStatus(code);
    response.out() << Wt::Json::serialize(json_error, API::INDENT);
    return;
  }

  //обработка запросов
  if (api_function.contains(API::ADD_STREAM))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    QString url = QString::fromStdString(json.get(API::P_URL).toString().orIfNull(""));
    QString callback_url = QString::fromStdString(json.get(API::P_CALLBACK_URL).toString().orIfNull(""));

    /*cout << "__API call addStream: streamId = " << vstream_ext.toStdString()
         << "; url = " << url.toStdString()
         << "; callback = " << callback_url.toStdString() << endl;*/
    singleton.addLog(QString("API call addStream: streamId = %1;  url = %2;  callback = %3").arg(vstream_ext, url, callback_url));

    std::vector<int> face_ids;
    if (!json.get(API::P_FACE_IDS).isNull() && json.get(API::P_FACE_IDS).type() != Wt::Json::Type::Array)
    {
      Wt::Json::Object json_error;
      int code = API::CODE_NOT_ACCEPTABLE;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.value(code);
      json_error[API::P_MESSAGE] = qUtf8Printable(API::INCORRECT_PARAMETER.arg(API::P_FACE_IDS));
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_error, API::INDENT);
      return;
    }
    Wt::Json::Array face_ids_list = json.get(API::P_FACE_IDS).orIfNull(Wt::Json::Array());
    for (auto&& json_value : face_ids_list)
    {
      int id_descriptor = json_value.toNumber().orIfNull(0);
      if (id_descriptor > 0)
        face_ids.push_back(id_descriptor);
    }

    QHash<QString, QString> params;
    if (!json.get(API::P_PARAMS).isNull() && json.get(API::P_PARAMS).type() != Wt::Json::Type::Array)
    {
      Wt::Json::Object json_error;
      int code = API::CODE_NOT_ACCEPTABLE;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.value(code);
      json_error[API::P_MESSAGE] = qUtf8Printable(API::INCORRECT_PARAMETER.arg(API::P_PARAMS));
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_error, API::INDENT);
      return;
    }
    Wt::Json::Array params_list = json.get(API::P_PARAMS).orIfNull(Wt::Json::Array());
    bool is_ok2 = true;
    for (auto&& json_value : params_list)
      if (json_value.type() == Wt::Json::Type::Object)
      {
        Wt::Json::Object json_object = json_value;
        QString param_name = QString::fromStdString(json_object.get(API::P_PARAM_NAME).toString().orIfNull(""));
        QString param_value = QString::fromStdString(json_object.get(API::P_PARAM_VALUE).toString().orIfNull(""));
        if (!param_name.isEmpty())
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
      Wt::Json::Object json_error;
      int code = API::CODE_NOT_ACCEPTABLE;
      json_error[API::P_CODE] = code;
      json_error[API::P_NAME] = API::RESPONSE_RESULT.value(code);
      json_error[API::P_MESSAGE] = qUtf8Printable(API::INCORRECT_PARAMETER.arg(API::P_PARAMS));
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_error, API::INDENT);
      return;
    }

    is_ok2 = addVStream(vstream_ext, url, callback_url, face_ids, params);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function.contains(API::MOTION_DETECTION))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_START))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    bool is_start = (QString::fromStdString(json.get(API::P_START).toString().orIfNull("")) == "t");

    /*cout << "__API call motionDetection: streamId = " << vstream_ext.toStdString()
         << "; isStart = " << is_start << endl;*/
    singleton.addLog(QString("API call motionDetection: streamId = %1;  isStart = %2").arg(vstream_ext).arg(is_start));

    motionDetection(vstream_ext, is_start);
    response.setStatus(API::CODE_NO_CONTENT);

    return;
  }

  if (api_function.contains(API::BEST_QUALITY))
  {
    if (json.get(API::P_LOG_FACES_ID).isNull() && (json.get(API::P_STREAM_ID).isNull() || json.get(API::P_DATE).isNull()))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    int id_vstream = 0;

    int id_log = 0;
    if (!json.get(API::P_LOG_FACES_ID).isNull())
      id_log = json.get(API::P_LOG_FACES_ID).toNumber().orIfNull(0);

    QDateTime log_date = QDateTime::fromString(QString::fromStdString(json.get(API::P_DATE).toString().orIfNull("")), API::DATETIME_FORMAT);
    if (id_log == 0 && !log_date.isValid())
    {
      simpleResponse(API::CODE_NOT_ACCEPTABLE, API::INCORRECT_PARAMETER.arg(API::P_DATE), response);
      return;
    }

    double interval_before = 0.0;
    double interval_after = 0.0;

    if (id_log == 0)
    {
      lock_guard<mutex> lock(singleton.mtx_task_config);
      id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
      if (id_vstream == 0)
        return;

      interval_before = singleton.task_config.vstream_conf_params[id_vstream][CONF_BEST_QUALITY_INTERVAL_BEFORE].value.toDouble();
      interval_after = singleton.task_config.vstream_conf_params[id_vstream][CONF_BEST_QUALITY_INTERVAL_AFTER].value.toDouble();
    }

    singleton.addLog(QString("API call bestQuality: eventId = %1;  streamId = %2;  date = %3").arg(id_log).arg(
      vstream_ext, QString::fromStdString(json.get(API::P_DATE).toString().orIfNull(""))));

    SQLGuard sql_guard;
    QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

    QSqlQuery q_get_log_faces(sql_db);
    QString sql_query = (id_log == 0) ? SQL_GET_LOG_FACE_BEST_QUALITY : SQL_GET_LOG_FACE_BY_ID;
    if (!q_get_log_faces.prepare(sql_query))
    {
      singleton.addLog(API::ERROR_SQL_PREPARE.arg((id_log == 0) ? "SQL_GET_LOG_FACE_BEST_QUALITY" : "SQL_GET_LOG_FACE_BY_ID"));
      singleton.addLog(q_get_log_faces.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    if (id_log == 0)
    {
      q_get_log_faces.bindValue(":id_vstream", id_vstream);
      q_get_log_faces.bindValue(":log_date", log_date);
      q_get_log_faces.bindValue(":interval_before", interval_before);
      q_get_log_faces.bindValue(":interval_after", interval_after);
    } else
      q_get_log_faces.bindValue(":id_log", id_log);

    q_get_log_faces.exec();
    if (q_get_log_faces.next())
    {
      QString screenshot = singleton.http_server_screenthot_url + q_get_log_faces.value("screenshot").toString();
      int face_left = q_get_log_faces.value("face_left").toInt();
      int face_top = q_get_log_faces.value("face_top").toInt();
      int face_width = q_get_log_faces.value("face_width").toInt();
      int face_height = q_get_log_faces.value("face_height").toInt();

      Wt::Json::Object json_data;
      json_data[API::P_SCREENSHOT] = qUtf8Printable(screenshot);
      json_data[API::P_FACE_LEFT] = face_left;
      json_data[API::P_FACE_TOP] = face_top;
      json_data[API::P_FACE_WIDTH] = face_width;
      json_data[API::P_FACE_HEIGHT] = face_height;

      int code = API::CODE_SUCCESS;
      Wt::Json::Object json_response;
      json_response[API::P_CODE] = code;
      json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
      json_response[API::P_MESSAGE] = API::MSG_DONE;
      json_response[API::P_DATA] = json_data;
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_response, API::INDENT);
    }

    return;
  }

  if (api_function.contains(API::REGISTER_FACE))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_URL))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    QString url = QString::fromStdString(json.get(API::P_URL).toString().orIfNull(""));
    int face_left = json.get(API::P_FACE_LEFT).toNumber().orIfNull(0);
    int face_top = json.get(API::P_FACE_TOP).toNumber().orIfNull(0);
    int face_width = json.get(API::P_FACE_WIDTH).toNumber().orIfNull(0);
    int face_height = json.get(API::P_FACE_HEIGHT).toNumber().orIfNull(0);
    int id_vstream = 0;
    QString comments;

    //scope for lock mutex
    {
      lock_guard<mutex> lock(singleton.mtx_task_config);
      id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
      if (id_vstream > 0)
        comments = singleton.task_config.vstream_conf_params[id_vstream][CONF_COMMENTS_URL_IMAGE_ERROR].value.toString();
    }

    if (id_vstream == 0)
      return;

    singleton.addLog(QString("API call registerFace: streamId = %1;  url = %2;  face = [%3, %4, %5, %6]").arg(vstream_ext, url).arg(
      face_left).arg(face_top).arg(face_width).arg(face_height));

    auto task_data = std::make_shared<TaskData>(id_vstream, TASK_GET_FRAME_FROM_URL);
    task_data->is_registration = true;
    task_data->frame_url = url;
    task_data->response_continuation = response.createContinuation();
    auto rd = std::make_shared<RegisterDescriptorResponse>();
    rd->id_vstream = id_vstream;
    rd->comments = comments;
    rd->face_left = face_left;
    rd->face_top = face_top;
    rd->face_width = face_width;
    rd->face_height = face_height;

    task_data->response_continuation->setData(rd);
    task_data->response_continuation->waitForMoreData();
    singleton.task_scheduler->addTask(task_data, 0.0);
    response.setStatus(API::CODE_SUCCESS);

    return;
  }

  if (api_function.contains(API::ADD_FACES) || api_function.contains(API::REMOVE_FACES))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_FACE_IDS))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    std::vector<int> face_ids;
    Wt::Json::Array face_ids_list = json.get(API::P_FACE_IDS).orIfNull(Wt::Json::Array());
    QString face_ids_list2;
    for (auto&& json_value : face_ids_list)
    {
      int id_descriptor = json_value.toNumber().orIfNull(0);
      if (id_descriptor > 0)
      {
        face_ids.push_back(id_descriptor);
        face_ids_list2 += QString::number(id_descriptor) + ", ";
      }
    }

    face_ids_list2.chop(2);
    singleton.addLog(QString("API call %1: streamId = %2;  faceIds = [%3]").arg(
      api_function.contains(API::ADD_FACES) ? "addFaces" : "removeFaces", vstream_ext, face_ids_list2));

    bool is_ok2 = true;
    if (api_function.contains(API::ADD_FACES))
      is_ok2 = addFaces(vstream_ext, face_ids);
    if (api_function.contains(API::REMOVE_FACES))
      is_ok2 = removeFaces(vstream_ext, face_ids);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function.contains(API::LIST_STREAMS))
  {
    singleton.addLog("API call listStreams");

    //для теста
    /*{
      lock_guard<mutex> lock(singleton.mtx_task_config);

      for (auto&& id_descriptor : singleton.id_vstream_to_id_descriptors[1022])
      {
        cout << id_descriptor << "  ";
      }
      cout << endl;
    }*/

    SQLGuard sql_guard;
    QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

    QSqlQuery q_vstreams(sql_db);
    if (!q_vstreams.prepare(SQL_GET_VIDEO_STREAMS))
    {
      singleton.addLog("  Ошибка: не удалось подготовить запрос SQL_GET_VIDEO_STREAMS.");
      singleton.addLog(q_vstreams.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }
    q_vstreams.bindValue(":id_worker", singleton.id_worker);
    if (!q_vstreams.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_VIDEO_STREAMS.");
      singleton.addLog(q_vstreams.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    QSqlQuery q_links_descriptor_vstream(sql_db);
    if (!q_links_descriptor_vstream.prepare(SQL_GET_LINKS_DESCRIPTOR_VSTREAM_BY_ID))
    {
      singleton.addLog("  Ошибка: не удалось подготовить запрос SQL_GET_LINKS_DESCRIPTOR_VSTREAM_BY_ID.");
      singleton.addLog(q_links_descriptor_vstream.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    Wt::Json::Array json_data;
    while (q_vstreams.next())
    {
      int id_vstream = q_vstreams.value("id_vstream").toInt();
      QString vstream_ext = q_vstreams.value("vstream_ext").toString();
      if (!vstream_ext.startsWith(API::VIRTUAL_VS))
      {
        Wt::Json::Object data;
        data[API::P_STREAM_ID] = qUtf8Printable(vstream_ext);

        q_links_descriptor_vstream.bindValue(":id_worker", singleton.id_worker);
        q_links_descriptor_vstream.bindValue(":id_vstream", id_vstream);
        if (!q_links_descriptor_vstream.exec())
        {
          singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_LINKS_DESCRIPTOR_VSTREAM_BY_ID.");
          singleton.addLog(q_links_descriptor_vstream.lastError().text());
          simpleResponse(API::CODE_SERVER_ERROR, response);
          return;
        }

        Wt::Json::Array faces;
        while (q_links_descriptor_vstream.next())
        {
          int id_descriptor = q_links_descriptor_vstream.value("id_descriptor").toInt();
          faces.push_back(id_descriptor);
        }
        if (!faces.empty())
          data[API::P_FACE_IDS] = faces;
        json_data.push_back(data);
      }
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      Wt::Json::Object json_response;
      json_response[API::P_CODE] = code;
      json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
      json_response[API::P_MESSAGE] = API::MSG_DONE;
      json_response[API::P_DATA] = json_data;
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_response, API::INDENT);
    }

    return;
  }

  if (api_function.contains(API::REMOVE_STREAM))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));

    //cout << "__API call removeStream: streamId = " << vstream_ext.toStdString() << endl;
    singleton.addLog(QString("API call removeStream: streamId = %1").arg(vstream_ext));

    if (!removeVStream(vstream_ext))
      simpleResponse(API::CODE_SERVER_ERROR, response);

    return;
  }

  //для теста
  if (api_function.contains(API::TEST_IMAGE))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    int id_vstream = 0;

    //scope for lick mutex
    {
      lock_guard<mutex> lock(singleton.mtx_task_config);
      id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
      if (id_vstream == 0)
        return;
    }

    cv::Mat frame = cv::imread(singleton.working_path.toStdString() + "/frame.jpg");
    if (!frame.empty())
    {
      auto task_data = std::make_shared<TaskData>(id_vstream, TASK_TEST, false, 0, std::move(frame));
      singleton.task_scheduler->addTask(task_data, 0.0);
    }

    return;
  }

  if (api_function.contains(API::LIST_ALL_FACES))
  {
    singleton.addLog("API call listAllFaces");

    SQLGuard sql_guard;
    QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

    QSqlQuery q_descriptors(SQL_GET_ALL_FACE_DESCRIPTORS, sql_db);
    if (!q_descriptors.exec())
    {
      singleton.addLog("  Ошибка: не удалось выполнить запрос SQL_GET_ALL_FACE_DESCRIPTORS.");
      singleton.addLog(q_descriptors.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    Wt::Json::Array json_data;
    while (q_descriptors.next())
    {
      int id_descriptor = q_descriptors.value("id_descriptor").toInt();
      json_data.push_back(id_descriptor);
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      Wt::Json::Object json_response;
      json_response[API::P_CODE] = code;
      json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
      json_response[API::P_MESSAGE] = API::MSG_DONE;
      json_response[API::P_DATA] = json_data;
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_response, API::INDENT);
    }

    return;
  }

  if (api_function.contains(API::DELETE_FACES))
  {
    if (!checkInputParam(json, response, API::P_FACE_IDS))
      return;

    std::vector<int> face_ids;
    Wt::Json::Array face_ids_list = json.get(API::P_FACE_IDS).orIfNull(Wt::Json::Array());
    QString face_ids_list2;
    for (auto&& json_value : face_ids_list)
    {
      int id_descriptor = json_value.toNumber().orIfNull(0);
      if (id_descriptor > 0)
      {
        face_ids.push_back(id_descriptor);
        face_ids_list2 += QString::number(id_descriptor) + ", ";
      }
    }
    face_ids_list2.chop(2);
    singleton.addLog(QString("API call %1: faceIds = [%2]").arg("deleteFaces", face_ids_list2));
    bool is_ok2 = deleteFaces(face_ids);
    int code = is_ok2 ? API::CODE_SUCCESS : API::CODE_SERVER_ERROR;
    simpleResponse(code, response);

    return;
  }

  if (api_function.contains(API::GET_EVENTS))
  {
    if (!checkInputParam(json, response, API::P_STREAM_ID))
      return;

    if (!checkInputParam(json, response, API::P_DATE_START))
      return;

    if (!checkInputParam(json, response, API::P_DATE_END))
      return;

    QDateTime date_start = QDateTime::fromString(QString::fromStdString(json.get(API::P_DATE_START).toString().orIfNull("")), API::DATETIME_FORMAT);
    if (!date_start.isValid())
    {
      simpleResponse(API::CODE_NOT_ACCEPTABLE, API::INCORRECT_PARAMETER.arg(API::P_DATE_START), response);
      return;
    }

    QDateTime date_end = QDateTime::fromString(QString::fromStdString(json.get(API::P_DATE_END).toString().orIfNull("")), API::DATETIME_FORMAT);
    if (!date_end.isValid())
    {
      simpleResponse(API::CODE_NOT_ACCEPTABLE, API::INCORRECT_PARAMETER.arg(API::P_DATE_END), response);
      return;
    }

    QString vstream_ext = QString::fromStdString(json.get(API::P_STREAM_ID).toString().orIfNull(""));
    int id_vstream = 0;

    //scope for lock mutex
    {
      lock_guard<mutex> lock(singleton.mtx_task_config);
      id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
      if (id_vstream == 0)
        return;
    }

    singleton.addLog(QString("API call getEvents: streamId = %1;  dateStart = %2;  dateEnd = %3").arg(vstream_ext,
      QString::fromStdString(json.get(API::P_DATE_START).toString().orIfNull("")),
      QString::fromStdString(json.get(API::P_DATE_END).toString().orIfNull(""))));

    SQLGuard sql_guard;
    QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

    QSqlQuery q_get_log_faces(sql_db);
    QString sql_query = SQL_GET_LOG_FACES_FROM_INTERVAL;
    if (!q_get_log_faces.prepare(sql_query))
    {
      singleton.addLog(API::ERROR_SQL_PREPARE.arg(SQL_GET_LOG_FACES_FROM_INTERVAL));
      singleton.addLog(q_get_log_faces.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    q_get_log_faces.bindValue(":id_vstream", id_vstream);
    q_get_log_faces.bindValue(":date_start", date_start);
    q_get_log_faces.bindValue(":date_end", date_end);
    if (!q_get_log_faces.exec())
    {
      singleton.addLog(API::ERROR_SQL_EXEC.arg(SQL_GET_LOG_FACES_FROM_INTERVAL));
      singleton.addLog(q_get_log_faces.lastError().text());
      simpleResponse(API::CODE_SERVER_ERROR, response);
      return;
    }

    Wt::Json::Array json_data;
    while (q_get_log_faces.next())
    {
      QDateTime log_date = q_get_log_faces.value("log_date").toDateTime();
      int id_descriptor = q_get_log_faces.value("id_descriptor").toInt();
      double quality = q_get_log_faces.value("quality").toDouble();
      QString screenshot = singleton.http_server_screenthot_url + q_get_log_faces.value("screenshot").toString();
      int face_left = q_get_log_faces.value("face_left").toInt();
      int face_top = q_get_log_faces.value("face_top").toInt();
      int face_width = q_get_log_faces.value("face_width").toInt();
      int face_height = q_get_log_faces.value("face_height").toInt();

      Wt::Json::Object json_item;
      json_item[API::P_DATE] = qUtf8Printable(log_date.toString(API::DATETIME_FORMAT_LOG_FACES));
      if (id_descriptor > 0)
        json_item[API::P_FACE_ID] = id_descriptor;
      json_item[API::P_QUALITY] = int(quality);
      json_item[API::P_SCREENSHOT] = qUtf8Printable(screenshot);
      json_item[API::P_FACE_LEFT] = face_left;
      json_item[API::P_FACE_TOP] = face_top;
      json_item[API::P_FACE_WIDTH] = face_width;
      json_item[API::P_FACE_HEIGHT] = face_height;

      json_data.push_back(json_item);
    }

    if (!json_data.empty())
    {
      int code = API::CODE_SUCCESS;
      Wt::Json::Object json_response;
      json_response[API::P_CODE] = code;
      json_response[API::P_NAME] = API::RESPONSE_RESULT[code];
      json_response[API::P_MESSAGE] = API::MSG_DONE;
      json_response[API::P_DATA] = json_data;
      response.setStatus(code);
      response.out() << Wt::Json::serialize(json_response, API::INDENT);
    }

    return;
  }

  Wt::Json::Object json_error;
  int code = API::CODE_ERROR;
  json_error[API::P_CODE] = code;
  json_error[API::P_NAME] = API::RESPONSE_RESULT[code];
  json_error[API::P_MESSAGE] = "Неизвестная API функция в запросе.";
  response.setStatus(code);
  response.out() << Wt::Json::serialize(json_error, API::INDENT);
}

bool ApiResource::addVStream(const QString& vstream_ext, const QString& url_new, const QString& callback_url_new,
  const std::vector<int>& face_ids, const QHash<QString, QString>& params)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return false;

  int id_vstream = 0;
  QString url;
  QString callback_url;
  int region_x = 0;
  int region_y = 0;
  int region_width = 0;
  int region_height = 0;
  QHash<QString, ConfParam> vstream_params;
  vstream_params.setSharable(false);

  QSet<int> id_vstream_descriptors;
  id_vstream_descriptors.setSharable(false);
  QHash<int, FaceDescriptor> id_descriptor_to_data;
  id_descriptor_to_data.setSharable(false);

  //scope for lock mutext
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
    if (id_vstream > 0)
    {
      url = singleton.task_config.conf_vstream_url[id_vstream];
      callback_url = singleton.task_config.conf_vstream_callback_url[id_vstream];
      if (singleton.task_config.conf_vstream_region.contains(id_vstream))
      {
        region_x = singleton.task_config.conf_vstream_region[id_vstream].x;
        region_y = singleton.task_config.conf_vstream_region[id_vstream].y;
        region_width = singleton.task_config.conf_vstream_region[id_vstream].width;
        region_height = singleton.task_config.conf_vstream_region[id_vstream].height;
      }
      id_vstream_descriptors = singleton.id_vstream_to_id_descriptors[id_vstream];
    }
    vstream_params = singleton.task_config.vstream_conf_params[id_vstream];
  }

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

  if (id_vstream == 0)
  {
    //проверяем, есть ли видеопоток в базе
    QSqlQuery q_get_vstream(sql_db);
    if (!q_get_vstream.prepare(SQL_GET_VIDEO_STREAM_BY_EXT))
    {
      singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_GET_VIDEO_STREAM_BY_EXT", __FUNCTION__));
      singleton.addLog(q_get_vstream.lastError().text());
      return false;
    }
    q_get_vstream.bindValue(":vstream_ext", vstream_ext);
    if (!q_get_vstream.exec())
    {
      singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_GET_VIDEO_STREAM_BY_EXT", __FUNCTION__));
      singleton.addLog(q_get_vstream.lastError().text());
      return false;
    }
    if (q_get_vstream.next())
    {
      id_vstream = q_get_vstream.value("id_vstream").toInt();
      url = q_get_vstream.value("url").toString();
      callback_url = q_get_vstream.value("callback_url").toString();
      region_x = q_get_vstream.value("region_x").toInt();
      region_y = q_get_vstream.value("region_y").toInt();
      region_width = q_get_vstream.value("region_width").toInt();
      region_height = q_get_vstream.value("region_height").toInt();
    }
  }

  if (!url_new.isEmpty())
    url = url_new;

  if (!callback_url_new.isEmpty())
    callback_url = callback_url_new;

  for (auto it = params.begin(); it != params.end(); ++it)
  {
    const QString& param_name = it.key();
    const QString& param_value = it.value();

    if (param_name == "region-x")
    {
      bool is_ok = true;
      int v = param_value.toInt(&is_ok);
      if (is_ok)
        region_x = v;
    }

    if (param_name == "region-y")
    {
      bool is_ok = true;
      int v = param_value.toInt(&is_ok);
      if (is_ok)
        region_y = v;
    }

    if (param_name == "region-width")
    {
      bool is_ok = true;
      int v = param_value.toInt(&is_ok);
      if (is_ok)
        region_width = v;
    }

    if (param_name == "region-height")
    {
      bool is_ok = true;
      int v = param_value.toInt(&is_ok);
      if (is_ok)
        region_height = v;
    }

    if (vstream_params.contains(param_name))
    {
      bool is_ok = true;
      QVariant v;
      switch (vstream_params[param_name].value.type())
      {
        case QVariant::Bool:
          v = static_cast<bool>(param_value.toInt(&is_ok));
          break;

        case QVariant::Int:
          v = param_value.toInt(&is_ok);
          break;

        case QVariant::Double:
          v = param_value.toDouble(&is_ok);
          break;

        default:
          v = param_value;
          break;
      }
      if (is_ok && v.isValid())
        vstream_params[param_name].value = v;
    }
  }

  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    singleton.addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  QSqlQuery q_vstream(sql_db);
  QString sql_query = (id_vstream == 0) ? SQL_ADD_VSTREAM : SQL_UPDATE_VSTREAM;
  if (!q_vstream.prepare(sql_query))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg(
      (id_vstream == 0) ? "SQL_INSERT_VSTREAM" : "SQL_UPDATE_VSTREAM", __FUNCTION__));
    singleton.addLog(q_vstream.lastError().text());
    return false;
  }
  if (id_vstream == 0)
    q_vstream.bindValue(":vstream_ext", vstream_ext);
  else
    q_vstream.bindValue(":id_vstream", id_vstream);
  q_vstream.bindValue(":url", url);
  q_vstream.bindValue(":callback_url", callback_url);
  q_vstream.bindValue(":region_x", region_x);
  q_vstream.bindValue(":region_y", region_y);
  q_vstream.bindValue(":region_width", region_width);
  q_vstream.bindValue(":region_height", region_height);
  if (!q_vstream.exec())
  {
    singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg(
      (id_vstream == 0) ? "SQL_INSERT_VSTREAM" : "SQL_UPDATE_VSTREAM", __FUNCTION__));
    singleton.addLog(q_vstream.lastError().text());
    return false;
  }

  if (id_vstream == 0)
    id_vstream = q_vstream.lastInsertId().toInt();

  QSqlQuery q_link_worker_vstream(sql_db);
  if (!q_link_worker_vstream.prepare(SQL_ADD_LINK_WORKER_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_ADD_LINK_WORKER_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_worker_vstream.lastError().text());
    return false;
  }
  q_link_worker_vstream.bindValue(":id_worker", singleton.id_worker);
  q_link_worker_vstream.bindValue(":id_vstream", id_vstream);
  if (!q_link_worker_vstream.exec())
  {
    singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_ADD_LINK_WORKER_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_worker_vstream.lastError().text());
    return false;
  }

  //пишем конфигурационные конфигурационные параметры
  QSqlQuery q_set_vstream_param(sql_db);
  if (!q_set_vstream_param.prepare(SQL_SET_VIDEO_STREAM_PARAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_SET_VIDEO_STREAM_PARAM", __FUNCTION__));
    singleton.addLog(q_set_vstream_param.lastError().text());
    return false;
  }

  for (auto it = vstream_params.begin(); it != vstream_params.end(); ++it)
  {
    //для теста
    //cout << QString("__set vstream param %1, %2, %3\n").arg(id_vstream).arg(it.key(), it.value().value.toString()).toStdString();

    q_set_vstream_param.bindValue(":id_vstream", id_vstream);
    q_set_vstream_param.bindValue(":param_name", it.key());
    q_set_vstream_param.bindValue(":param_value", it.value().value.toString());
    if (!q_set_vstream_param.exec())
    {
      singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_SET_VIDEO_STREAM_PARAM", __FUNCTION__));
      singleton.addLog(q_set_vstream_param.lastError().text());
      return false;
    }
  }

  //привязываем дескрипторы
  QSqlQuery q_descriptor(sql_db);
  if (!q_descriptor.prepare(SQL_GET_FACE_DESCRIPTOR_BY_ID))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_GET_FACE_DESCRIPTOR_BY_ID", __FUNCTION__));
    singleton.addLog(q_descriptor.lastError().text());
    return false;
  }

  QSqlQuery q_link_descriptor_vstream(sql_db);
  if (!q_link_descriptor_vstream.prepare(SQL_ADD_LINK_DESCRIPTOR_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_ADD_LINK_DESCRIPTOR_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_descriptor_vstream.lastError().text());
    return false;
  }

  for (auto&& id_descriptor : face_ids)
  {
    //для теста
    //cout << "__descriptor " << id_descriptor;

    q_descriptor.bindValue(":id_descriptor", id_descriptor);
    if (!q_descriptor.exec())
    {
      singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_GET_FACE_DESCRIPTOR_BY_ID", __FUNCTION__));
      singleton.addLog(q_descriptor.lastError().text());
      return false;
    }
    if (q_descriptor.next())
      if (q_descriptor.value("id_descriptor").toInt() == id_descriptor && id_descriptor > 0)
      {

        //для теста
        //cout << " found.\n";

        if (!id_vstream_descriptors.contains(id_descriptor))
        {
          FaceDescriptor fd;
          fd.create(1, singleton.descriptor_size, CV_32F);
          QByteArray ba = q_descriptor.value("descriptor_data").toByteArray();
          std::memcpy(fd.data, ba.data(), ba.size());
          double norm_l2 = cv::norm(fd, cv::NORM_L2);
          if (norm_l2 <= 0.0)
            norm_l2 = 1.0;
          fd = fd / norm_l2;
          id_descriptor_to_data[id_descriptor] = std::move(fd);
        }

        q_link_descriptor_vstream.bindValue(":id_descriptor", id_descriptor);
        q_link_descriptor_vstream.bindValue(":id_vstream", id_vstream);
        q_link_descriptor_vstream.exec();
      }
      //для теста
      /*else
        cout << " not found.\n";*/
  }

  if (!sql_transaction.commit())
  {
    singleton.addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
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
      singleton.task_config.conf_vstream_region.remove(id_vstream);

    for (auto it = id_descriptor_to_data.begin(); it != id_descriptor_to_data.end(); ++it)
    {
      int id_descriptor = it.key();

      //для теста
      //cout << "__new descriptor for stream: " << id_descriptor << endl;

      if (!singleton.id_descriptor_to_data.contains(id_descriptor))
      {
        singleton.id_descriptor_to_data[id_descriptor] = std::move(it.value());
        cout << "  __descriptor loaded from database.\n";
      }
      singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
      singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
    }

    singleton.tp_task_config = std::chrono::steady_clock::now();
  }

  return true;
}

void ApiResource::motionDetection(const QString& vstream_ext, bool is_start)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
      return;

  int id_vstream = 0;

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
  }

  if (id_vstream == 0)
    return;

  bool do_task = false;
  auto now = std::chrono::steady_clock::now();

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_capture);
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

  if (do_task)
  {
    auto task_data = std::make_shared<TaskData>(id_vstream, TASK_CAPTURE_VSTREAM_FRAME);
    singleton.task_scheduler->addTask(task_data, 0.0);
    singleton.addLog(QString("Зафиксировано движение. Начало захвата кадров с видео потока %1").arg(task_data->id_vstream));
  }

}

bool ApiResource::addFaces(const QString& vstream_ext, const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return false;

  int id_vstream = 0;

  //scope for lock mutext
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
  }

  if (id_vstream == 0)
    return true;

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    singleton.addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  QSet<int> id_descriptors;
  QSqlQuery q_link_descriptor_vstream(sql_db);
  if (!q_link_descriptor_vstream.prepare(SQL_ADD_LINK_DESCRIPTOR_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_ADD_LINK_DESCRIPTOR_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_descriptor_vstream.lastError().text());
    return false;
  }

  for (auto&& id_descriptor : face_ids)
  {
    q_link_descriptor_vstream.bindValue(":id_descriptor", id_descriptor);
    q_link_descriptor_vstream.bindValue(":id_vstream", id_vstream);
    q_link_descriptor_vstream.exec();
    if (q_link_descriptor_vstream.numRowsAffected() > 0)
      id_descriptors.insert(id_descriptor);
  }

  if (!sql_transaction.commit())
  {
    singleton.addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  std::vector<int> id_new_descriptors;

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);

    for (auto&& id_descriptor : id_descriptors)
      if (singleton.id_descriptor_to_data.contains(id_descriptor))
      {
        singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
        singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
      } else
        id_new_descriptors.push_back(id_descriptor);
  }

  if (!id_new_descriptors.empty())  //привязываем новые дескрипторы, которые есть в базе, но нет в оперативной памяти
  {
    QHash<int, FaceDescriptor> id_descriptor_to_data;
    id_descriptor_to_data.setSharable(false);

    sql_transaction.start();

    QSqlQuery q_descriptor(sql_db);
    if (!q_descriptor.prepare(SQL_GET_FACE_DESCRIPTOR_BY_ID))
    {
      singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_GET_FACE_DESCRIPTOR_BY_ID", __FUNCTION__));
      singleton.addLog(q_descriptor.lastError().text());
      return false;
    }

    if (!q_link_descriptor_vstream.prepare(SQL_ADD_LINK_DESCRIPTOR_VSTREAM))
    {
      singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_ADD_LINK_DESCRIPTOR_VSTREAM", __FUNCTION__));
      singleton.addLog(q_link_descriptor_vstream.lastError().text());
      return false;
    }

    for (auto&& id_descriptor : id_new_descriptors)
    {
      q_descriptor.bindValue(":id_descriptor", id_descriptor);
      if (!q_descriptor.exec())
      {
        singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_GET_FACE_DESCRIPTOR_BY_ID", __FUNCTION__));
        singleton.addLog(q_descriptor.lastError().text());
        return false;
      }

      if (q_descriptor.next())
        if (q_descriptor.value("id_descriptor").toInt() == id_descriptor && id_descriptor > 0)
        {
          //для теста
          cout << "load from db descriptor: " << id_descriptor << endl;

          FaceDescriptor fd;
          fd.create(1, singleton.descriptor_size, CV_32F);
          QByteArray ba = q_descriptor.value("descriptor_data").toByteArray();
          std::memcpy(fd.data, ba.data(), ba.size());
          double norm_l2 = cv::norm(fd, cv::NORM_L2);
          if (norm_l2 <= 0.0)
            norm_l2 = 1.0;
          fd = fd / norm_l2;
          id_descriptor_to_data[id_descriptor] = std::move(fd);

          q_link_descriptor_vstream.bindValue(":id_descriptor", id_descriptor);
          q_link_descriptor_vstream.bindValue(":id_vstream", id_vstream);
          q_link_descriptor_vstream.exec();
        }
    }

    if (!id_descriptor_to_data.empty())
    {
      lock_guard<mutex> lock(singleton.mtx_task_config);
      for (auto it = id_descriptor_to_data.begin(); it != id_descriptor_to_data.end(); ++it)
      {
        int id_descriptor = it.key();
        singleton.id_descriptor_to_data[id_descriptor] = std::move(it.value());
        singleton.id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
        singleton.id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
      }

      singleton.tp_task_config = std::chrono::steady_clock::now();
    }
  }

  return true;
}

bool ApiResource::removeFaces(const QString& vstream_ext, const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return false;

  int id_vstream = 0;

  //scope for lock mutext
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
  }

  if (id_vstream == 0)
    return true;

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    singleton.addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  QSet<int> id_descriptors;
  QSet<int> id_descriptors_remove;
  QSqlQuery q_link_descriptor_vstream(sql_db);
  if (!q_link_descriptor_vstream.prepare(SQL_REMOVE_LINK_DESCRIPTOR_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_REMOVE_LINK_DESCRIPTOR_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_descriptor_vstream.lastError().text());
    return false;
  }

  QSqlQuery q_check_link(sql_db);
  if (!q_check_link.prepare(SQL_CHECK_LINKS_DESCRIPTOR_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_CHECK_LINKS_DESCRIPTOR_VSTREAM", __FUNCTION__));
    singleton.addLog(q_check_link.lastError().text());
    return false;
  }

  for (auto&& id_descriptor : face_ids)
  {
    q_link_descriptor_vstream.bindValue(":id_descriptor", id_descriptor);
    q_link_descriptor_vstream.bindValue(":id_vstream", id_vstream);
    q_link_descriptor_vstream.exec();
    if (q_link_descriptor_vstream.numRowsAffected() > 0)
    {
      id_descriptors.insert(id_descriptor);
      q_check_link.bindValue(":id_worker", singleton.id_worker);
      q_check_link.bindValue(":id_descriptor", id_descriptor);
      if (!q_check_link.exec())
      {
        singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_CHECK_LINKS_DESCRIPTOR_VSTREAM", __FUNCTION__));
        singleton.addLog(q_check_link.lastError().text());
        return false;
      }
      bool do_remove = !q_check_link.next();
      if (do_remove)
        id_descriptors_remove.insert(id_descriptor);
    }
  }

  if (!sql_transaction.commit())
  {
    singleton.addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);

    for (auto&& id_descriptor : id_descriptors)
    {
      singleton.id_descriptor_to_id_vstreams[id_descriptor].remove(id_vstream);
      if (singleton.id_descriptor_to_id_vstreams[id_descriptor].empty())
        singleton.id_descriptor_to_id_vstreams.remove(id_descriptor);
      singleton.id_vstream_to_id_descriptors[id_vstream].remove(id_descriptor);
      if (id_descriptors_remove.contains(id_descriptor))
        singleton.id_descriptor_to_data.remove(id_descriptor);
    }
  }

  return true;
}

bool ApiResource::deleteFaces(const std::vector<int>& face_ids)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return false;

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    singleton.addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  QSqlQuery q_remove_face_descriptor(sql_db);
  if (!q_remove_face_descriptor.prepare(SQL_REMOVE_FACE_DESCRIPTOR))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_REMOVE_FACE_DESCRIPTOR", __FUNCTION__));
    singleton.addLog(q_remove_face_descriptor.lastError().text());
    return false;
  }

  for (auto&& id_descriptor : face_ids)
  {
    q_remove_face_descriptor.bindValue(":id_descriptor", id_descriptor);
    if (!q_remove_face_descriptor.exec())
    {
      singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_REMOVE_FACE_DESCRIPTOR", __FUNCTION__));
      singleton.addLog(q_remove_face_descriptor.lastError().text());
      return false;
    }
  }

  if (!sql_transaction.commit())
  {
    singleton.addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);

    for (auto&& id_descriptor : face_ids)
    {
      //удаляем привязку к видеопотокам
      for (auto it = singleton.id_descriptor_to_id_vstreams[id_descriptor].constBegin(); it != singleton.id_descriptor_to_id_vstreams[id_descriptor].constEnd(); ++it)
        singleton.id_vstream_to_id_descriptors[*it].remove(id_descriptor);
      singleton.id_descriptor_to_id_vstreams.remove(id_descriptor);

      //удаляем дескриптор
      singleton.id_descriptor_to_data.remove(id_descriptor);
    }
  }

  return true;
}

bool ApiResource::removeVStream(const QString& vstream_ext)
{
  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return false;

  int id_vstream = 0;

  //scope for lock mutext
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    id_vstream = singleton.task_config.conf_vstream_ext_to_id_vstream.value(vstream_ext);
  }

  if (id_vstream == 0)
    return true;

  //для теста
  //cout << "__id_vstream: " << id_vstream << endl;

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());

  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    singleton.addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  QSet<int> id_descriptors_remove;
  QSqlQuery q_link_descriptor_vstream(sql_db);
  if (!q_link_descriptor_vstream.prepare(SQL_GET_SOLE_LINK_DESCRIPTORS_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_GET_SOLE_LINK_DESCRIPTORS_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_descriptor_vstream.lastError().text());
    return false;
  }
  q_link_descriptor_vstream.bindValue(":id_vstream", id_vstream);
  if (!q_link_descriptor_vstream.exec())
  {
    singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_GET_SOLE_LINK_DESCRIPTORS_VSTREAM", __FUNCTION__));
    singleton.addLog(q_link_descriptor_vstream.lastError().text());
    return false;
  }
  while (q_link_descriptor_vstream.next())
  {
    id_descriptors_remove.insert(q_link_descriptor_vstream.value("id_descriptor").toInt());

    //для теста
    //cout << "__descriptor to remove: " << q_link_descriptor_vstream.value("id_descriptor").toInt() << endl;
  }

  QSqlQuery q_remove_link_worker_vstream(sql_db);
  if (!q_remove_link_worker_vstream.prepare(SQL_REMOVE_LINK_WORKER_VSTREAM))
  {
    singleton.addLog(API::ERROR_SQL_PREPARE_IN_FUNCTION.arg("SQL_REMOVE_LINK_WORKER_VSTREAM", __FUNCTION__));
    singleton.addLog(q_remove_link_worker_vstream.lastError().text());
    return false;
  }
  q_remove_link_worker_vstream.bindValue(":id_worker", singleton.id_worker);
  q_remove_link_worker_vstream.bindValue(":id_vstream", id_vstream);
  if (!q_remove_link_worker_vstream.exec())
  {
    singleton.addLog(API::ERROR_SQL_EXEC_IN_FUNCTION.arg("SQL_REMOVE_LINK_WORKER_VSTREAM", __FUNCTION__));
    singleton.addLog(q_remove_link_worker_vstream.lastError().text());
    return false;
  }

  if (!sql_transaction.commit())
  {
    singleton.addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    singleton.addLog(sql_db.lastError().text());
    return false;
  }

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_capture);
    singleton.id_vstream_to_start_motion.remove(id_vstream);
    singleton.id_vstream_to_end_motion.remove(id_vstream);
    singleton.is_capturing.remove(id_vstream);
  }

  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);

    //удаляем привязки дескрипторов к видеопотоку и ненужные дескрипторы
    singleton.id_vstream_to_id_descriptors.remove(id_vstream);
    for (auto&& id_descriptor : id_descriptors_remove)
      singleton.id_descriptor_to_data.remove(id_descriptor);

    //удаляем конфиги видео потока
    singleton.task_config.conf_id_vstream_to_vstream_ext.remove(id_vstream);
    singleton.task_config.conf_vstream_ext_to_id_vstream.remove(vstream_ext);
    singleton.task_config.conf_vstream_url.remove(id_vstream);
    singleton.task_config.conf_vstream_callback_url.remove(id_vstream);
    singleton.task_config.conf_vstream_region.remove(id_vstream);
    singleton.task_config.vstream_conf_params.remove(id_vstream);
  }

  return true;
}

ApiResource::~ApiResource()
{
  beingDeleted();
}
