#include <utility>

#include "singleton.h"
#include "frs_api.h"
#include "tasks.h"
#include "crow/TinySHA1.hpp"

using namespace std;

namespace
{
  mutex mtx_log;
}

double Singleton::cosineDistance(const FaceDescriptor& fd1, const FaceDescriptor& fd2)
{
  if (fd1.cols != fd2.cols || fd1.cols == 0)
    return -1.0;

  return fd1.dot(fd2);
}

Singleton::Singleton()
{
  std::srand(std::clock());
}

Singleton::~Singleton()
{
  sql_client.reset();
  sql_client = nullptr;
  saveDNNStatsData();
}

//логи
void Singleton::addLog(const String &msg) const
{
  scoped_lock locker(mtx_log);

  //выводим лог в консоль
  auto dt = absl::Now();
  auto s_log = absl::Substitute("[$0] $1\n", absl::FormatTime("%H:%M:%S", dt, absl::LocalTimeZone()), msg);
  cout << s_log;

  //пишем лог в конец файла
  std::filesystem::path path = logs_path / (absl::FormatTime("%Y-%m-%d", dt, absl::LocalTimeZone()) + ".log");
  std::filesystem::create_directories(logs_path);
  std::fstream f(path, std::ios::app);
  f << s_log;
}

void Singleton::loadData()
{
  try
  {
    auto mysql_session = sql_client->getSession();
    auto result = mysql_session.sql(SQL_GET_FACE_DESCRIPTORS)
      .bind(id_worker)
      .execute();
    for (auto row : result)
    {
      int id_descriptor = row[0];
      FaceDescriptor fd;
      fd.create(1, int(descriptor_size), CV_32F);
      std::string s = row[1].get<std::string>();
      std::memcpy(fd.data, s.data(), s.size());
      double norm_l2 = cv::norm(fd, cv::NORM_L2);
      if (norm_l2 <= 0.0)
        norm_l2 = 1.0;
      fd = fd / norm_l2;
      id_descriptor_to_data[id_descriptor] = std::move(fd);
    }
  } catch (const mysqlx::Error& err)
  {
    addLog(absl::Substitute(ERROR_SQL_EXEC_IN_FUNCTION, "SQL_GET_FACE_DESCRIPTORS", __FUNCTION__));
    addLog(err.what());
    return;
  }
}

int Singleton::addFaceDescriptor(int id_vstream, const FaceDescriptor& fd, const cv::Mat& f_img)
{
  auto mysql_session = sql_client->getSession();
  int id_descriptor{};
  try
  {
    mysql_session.startTransaction();

    //сохраняем дескриптор и изображение
    id_descriptor = static_cast<int>(mysql_session.sql(SQL_ADD_FACE_DESCRIPTOR)
      .bind(mysqlx::bytes(fd.data, descriptor_size * sizeof(float)))
      .execute()
      .getAutoIncrementValue());

    std::vector<uchar> buff_;
    cv::imencode(".jpg", f_img, buff_);
    mysql_session.sql(SQL_ADD_DESCRIPTOR_IMAGE)
      .bind(id_descriptor)
      .bind("image/jpeg")
      .bind(mysqlx::bytes(buff_.data(), buff_.size()))
      .execute();

    mysql_session.sql(SQL_ADD_LINK_DESCRIPTOR_VSTREAM)
      .bind(id_descriptor)
      .bind(id_vstream)
      .execute();

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    addLog(err.what());
    return {};
  }

  //scope for lock mutex
  {
    double norm_l2 = cv::norm(fd, cv::NORM_L2);
    if (norm_l2 <= 0.0)
      norm_l2 = 1.0;

    WriteLocker lock(mtx_task_config);
    id_descriptor_to_data[id_descriptor] = fd.clone() / norm_l2;
    id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
    id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
  }

  return id_descriptor;
}

std::tuple<int, String, String> Singleton::addLogFace(int id_vstream, DateTime log_date, int id_descriptor, double quality,
   const cv::Rect& face_rect, const string& screenshot) const
{
  sha1::SHA1 s;
  auto hash_name = absl::StrCat(absl::FormatTime(log_date), id_vstream);
  s.processBytes(hash_name.data(), hash_name.size());
  uint8_t digest[20];
  s.getDigestBytes(digest);
  hash_name = absl::BytesToHexString(std::string((const char *)(digest), 16));
  auto url_part = absl::Substitute("$0/$1/$2/$3", hash_name[0], hash_name[1], hash_name[2], hash_name);
  String image_ext = ".jpg";
  auto file_path = screenshot_path / (url_part + image_ext);

  //для теста
  //auto tt0 = std::chrono::steady_clock::now();

  auto mysql_session = sql_client->getSession();
  int id_log;
  try
  {
    mysql_session.startTransaction();

    id_log = static_cast<int>(mysql_session.sql(SQL_ADD_LOG_FACE)
      .bind(id_vstream)
      .bind(absl::FormatTime(API::DATETIME_FORMAT_LOG_FACES, log_date, absl::LocalTimeZone()))
      .bind(id_descriptor > 0 ? mysqlx::Value(id_descriptor) : mysqlx::nullvalue)
      .bind(quality)
      .bind(url_part + image_ext)
      .bind(face_rect.x)
      .bind(face_rect.y)
      .bind(face_rect.width)
      .bind(face_rect.height)
      .execute()
      .getAutoIncrementValue());

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    addLog(err.what());
    return {};
  }

  //для теста
  //auto tt1 = std::chrono::steady_clock::now();
  //cout << "____write log_face time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << " ms" << endl;

  //пишем файл со скриншотом
  std::filesystem::create_directories(file_path.parent_path());
  ofstream f(file_path);
  f << screenshot;

  //для теста
  //auto tt2 = std::chrono::steady_clock::now();
  //cout << "____write screenshot time: " << std::chrono::duration<double, std::milli>(tt2 - tt1).count() << " ms" << endl;

  return {id_log, hash_name, http_server_screenshot_url + url_part + image_ext};
}

void Singleton::addLogDeliveryEvent(DeliveryEventType delivery_type, DeliveryEventResult delivery_result, int id_vstream,
  int id_descriptor) const
{
  auto mysql_session = sql_client->getSession();
  try
  {
    mysql_session.startTransaction();

    mysql_session.sql(SQL_ADD_LOG_DELIVERY_EVENT)
      .bind(static_cast<int>(delivery_type))
      .bind(static_cast<int>(delivery_result))
      .bind(id_vstream)
      .bind(id_descriptor)
      .execute();

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    addLog(err.what());
    return;
  }
}

void Singleton::addPermanentTasks() const
{
  runtime->background_executor()->submit(removeOldLogFaces, false);
}

String Singleton::variantToString(const Variant& v)
{
  if (std::holds_alternative<bool>(v))
    return std::to_string(static_cast<int>(std::get<bool>(v)));

  if (std::holds_alternative<int>(v))
    return std::to_string(std::get<int>(v));

  if (std::holds_alternative<double>(v))
    return std::to_string(std::get<double>(v));

  if (std::holds_alternative<String>(v))
    return std::get<String>(v);

  return {};
}

int Singleton::addSGroupFaceDescriptor(int id_sgroup, const FaceDescriptor& fd, const cv::Mat& f_img)
{
  auto mysql_session = sql_client->getSession();
  int id_descriptor{};
  try
  {
    mysql_session.startTransaction();

    //сохраняем дескриптор и изображение
    id_descriptor = static_cast<int>(mysql_session.sql(SQL_ADD_FACE_DESCRIPTOR)
      .bind(mysqlx::bytes(fd.data, descriptor_size * sizeof(float)))
      .execute()
      .getAutoIncrementValue());

    std::vector<uchar> buff_;
    cv::imencode(".jpg", f_img, buff_);
    mysql_session.sql(SQL_ADD_DESCRIPTOR_IMAGE)
      .bind(id_descriptor)
      .bind("image/jpeg")
      .bind(mysqlx::bytes(buff_.data(), buff_.size()))
      .execute();

    mysql_session.sql(SQL_ADD_LINK_DESCRIPTOR_SGROUP)
      .bind(id_descriptor)
      .bind(id_sgroup)
      .execute();

    mysql_session.commit();
  } catch (const mysqlx::Error& err)
  {
    mysql_session.rollback();
    addLog(err.what());
    return {};
  }

  //scope for lock mutex
  {
    double norm_l2 = cv::norm(fd, cv::NORM_L2);
    if (norm_l2 <= 0.0)
      norm_l2 = 1.0;

    WriteLocker lock(mtx_task_config);
    id_descriptor_to_data[id_descriptor] = fd.clone() / norm_l2;
    id_sgroup_to_id_descriptors[id_sgroup].insert(id_descriptor);
  }

  return id_descriptor;
}

//для сбора статистики инференса
void Singleton::loadDNNStatsData()
{
  auto file_name = working_path + "/dnn_stats_data.json";
  error_code ec;
  const auto f_size = std::filesystem::file_size(file_name, ec);
  if (!ec && f_size > 0)
  {
    HashMap<int, DNNStatsData> dnn_stats_data_copy;
    string s_data(f_size, '\0');
    ifstream f(file_name, std::ios::in | std::ios::binary);
    f.read(s_data.data(), static_cast<streamsize>(f_size));
    f.close();
    crow::json::rvalue json = crow::json::load(s_data);
    if (json.has("data") && json["data"].t() == crow::json::type::List)
    {
      auto data = json["data"];
      auto list_data = data.lo();
      for (auto & item : list_data)
        if (item.has("id_vstream") && item["id_vstream"].t() == crow::json::type::Number
          && item.has("fd_count") && item["fd_count"].t() == crow::json::type::Number
          && item.has("fc_count") && item["fc_count"].t() == crow::json::type::Number
          && item.has("fr_count") && item["fr_count"].t() == crow::json::type::Number)
        {
          dnn_stats_data_copy[static_cast<int>(item["id_vstream"].i())] = DNNStatsData{
            static_cast<int>(item["fd_count"].i()),
            static_cast<int>(item["fc_count"].i()),
            static_cast<int>(item["fr_count"].i())
          };
        }
    }

    //scope for locked mutex
    {
      scoped_lock lock(mtx_stats);
      dnn_stats_data = dnn_stats_data_copy;
    }
  }
}

//сохраняем данные статистики инференса в файл
void Singleton::saveDNNStatsData()
{
  HashMap<int, DNNStatsData> dnn_stats_data_copy;

  //scope for lock mutex
  {
    scoped_lock lock(mtx_stats);
    dnn_stats_data_copy = dnn_stats_data;
  }

  DNNStatsData all_data;
  crow::json::wvalue::list list_data;
  for (const auto& item : dnn_stats_data_copy)
  {
    all_data.fd_count += item.second.fd_count;
    all_data.fc_count += item.second.fc_count;
    all_data.fr_count += item.second.fr_count;
    list_data.push_back({
      {"id_vstream", item.first},
      {"fd_count", item.second.fd_count},
      {"fc_count", item.second.fc_count},
      {"fr_count", item.second.fr_count},
    });
  }
  crow::json::wvalue  json{
    {"all", {{"fd_count", all_data.fd_count}, {"fc_count", all_data.fc_count}, {"fr_count", all_data.fr_count}}},
    {"data", list_data}
  };

  ofstream f(working_path + "/dnn_stats_data.json");
  f << json.dump();
}
