#include <Wt/WServer.h>

#include <utility>

#include "singleton.h"
#include "frs_api.h"

namespace
{
  QReadWriteLock lock_log;
}

double Singleton::cosineDistance(const FaceDescriptor& fd1, const FaceDescriptor& fd2)
{
  if (fd1.cols != fd2.cols || fd1.cols == 0)
    return -1.0;

  return fd1.dot(fd2);
}

//Обработка транзакции

SqlTransaction::SqlTransaction(QString connection_name, bool do_start)
  : conn_name(std::move(connection_name))
{
  if (do_start)
  {
    QSqlDatabase db = QSqlDatabase::database(conn_name);
    in_transaction = db.transaction();
  }
}

SqlTransaction::~SqlTransaction()
{
  rollback();
}

bool SqlTransaction::start()
{
  if (!in_transaction)
  {
    QSqlDatabase db = QSqlDatabase::database(conn_name);
    in_transaction = db.transaction();
  }

  return in_transaction;
}

bool SqlTransaction::commit()
{
  if (in_transaction)
  {
    in_transaction = false;
    QSqlDatabase db = QSqlDatabase::database(conn_name);
    return db.commit();
  } else
    return false;
}

bool SqlTransaction::rollback()
{
  if (in_transaction)
  {
    in_transaction = false;
    QSqlDatabase db = QSqlDatabase::database(conn_name);
    return db.rollback();
  } else
    return true;
}

bool SqlTransaction::inTransaction() const
{
  return in_transaction;
}

QSqlDatabase SqlTransaction::getDatabase()
{
  return QSqlDatabase::database(conn_name);
}

Singleton::Singleton()
{
  std::srand(unsigned(std::time(nullptr)));
  id_descriptor_to_data.setSharable(false);
  id_vstream_to_id_descriptors.setSharable(false);
  id_descriptor_to_id_vstreams.setSharable(false);
}

Singleton::~Singleton()
{
  task_scheduler.reset();
  task_scheduler = nullptr;
  sql_pool.reset();
  sql_pool = nullptr;
  addLog("Закрытие программы.");
}

//логи
void Singleton::addLog(const QString &msg) const
{
  QWriteLocker locker(&lock_log);

  //выводим лог в консоль
  QDateTime dt = QDateTime::currentDateTime();
  QString s_log = QString("[%1] %2\n").arg(dt.toString("HH:mm:ss.zzz"), msg);
  cout << qUtf8Printable(s_log);

  //пишем лог в конец файла
  QDir d;
  d.mkpath(logs_path);
  QString f_name = QString("%1/%2.txt").arg(logs_path, dt.toString("yyyy-MM-dd"));
  QFile f(f_name);
  f.open(QFile::WriteOnly | QFile::Append | QFile::Text);
  f.write(s_log.toUtf8());
  f.close();
}

void Singleton::loadData()
{
  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());
  QSqlQuery q_face_descriptors(sql_db);
  if (!q_face_descriptors.prepare(SQL_GET_FACE_DESCRIPTORS))
  {
    addLog("Ошибка: не удалось подготовить запрос SQL_GET_FACE_DESCRIPTORS.");
    addLog(q_face_descriptors.lastError().text());
    return;
  }
  q_face_descriptors.bindValue(":id_worker", id_worker);
  if (!q_face_descriptors.exec())
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_GET_FACE_DESCRIPTORS.");
    addLog(q_face_descriptors.lastError().text());
    return;
  }

  while (q_face_descriptors.next())
  {
    int id_descriptor = q_face_descriptors.value("id_descriptor").toInt();
    FaceDescriptor fd;
    fd.create(1, descriptor_size, CV_32F);
    QByteArray ba = q_face_descriptors.value("descriptor_data").toByteArray();
    std::memcpy(fd.data, ba.data(), ba.size());
    double norm_l2 = cv::norm(fd, cv::NORM_L2);
    if (norm_l2 <= 0.0)
      norm_l2 = 1.0;
    fd = fd / norm_l2;
    id_descriptor_to_data[id_descriptor] = std::move(fd);
  }
}

int Singleton::addFaceDescriptor(int id_vstream, const FaceDescriptor& fd, const cv::Mat& f_img)
{
  int id_descriptor{};

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());
  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    addLog(sql_db.lastError().text());
    return {};
  }

  //сохраняем дескриптор и изображение
  QSqlQuery q_add_face_descriptor(sql_db);
  if (!q_add_face_descriptor.prepare(SQL_ADD_FACE_DESCRIPTOR))
  {
    addLog("Ошибка: не удалось подготовить запрос SQL_ADD_FACE_DESCRIPTOR.");
    addLog(q_add_face_descriptor.lastError().text());
    return {};
  }
  q_add_face_descriptor.bindValue(":descriptor_data",
    QByteArray((char*)fd.data, descriptor_size * sizeof(float)), QSql::In | QSql::Binary);
  if (!q_add_face_descriptor.exec())
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_ADD_FACE_DESCRIPTOR.");
    addLog(q_add_face_descriptor.lastError().text());
    return {};
  }
  id_descriptor = q_add_face_descriptor.lastInsertId().toInt();

  QSqlQuery q_add_descriptor_image(sql_db);
  if (!q_add_descriptor_image.prepare(SQL_ADD_DESCRIPTOR_IMAGE))
  {
    addLog("Ошибка: не удалось подготовить запрос SQL_ADD_DESCRIPTOR_IMAGE.");
    addLog(q_add_descriptor_image.lastError().text());
    return {};
  }
  q_add_descriptor_image.bindValue(":id_descriptor", id_descriptor);
  q_add_descriptor_image.bindValue(":mime_type", "image/jpeg");
  std::vector<uchar> buff_;
  cv::imencode(".jpg", f_img, buff_);
  q_add_descriptor_image.bindValue(":face_image", QByteArray((char*)buff_.data(), buff_.size()), QSql::In | QSql::Binary);
  if (!q_add_descriptor_image.exec())
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_ADD_DESCRIPTOR_IMAGE.");
    addLog(q_add_descriptor_image.lastError().text());
    return {};
  }

  QSqlQuery q_link_descriptor_vstream(sql_db);
  if (!q_link_descriptor_vstream.prepare(SQL_ADD_LINK_DESCRIPTOR_VSTREAM))
  {
    addLog(QString("Ошибка: не удалось подготовить запрос %1 в функции %2.").arg("SQL_ADD_LINK_DESCRIPTOR_VSTREAM", __FUNCTION__));
    addLog(q_link_descriptor_vstream.lastError().text());
    return {};
  }
  q_link_descriptor_vstream.bindValue(":id_descriptor", id_descriptor);
  q_link_descriptor_vstream.bindValue(":id_vstream", id_vstream);
  q_link_descriptor_vstream.exec();

  if (!sql_transaction.commit())
  {
    addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    addLog(sql_transaction.getDatabase().lastError().text());
    return {};
  }

  //scope for lock mutex
  {
    double norm_l2 = cv::norm(fd, cv::NORM_L2);
    if (norm_l2 <= 0.0)
      norm_l2 = 1.0;

    lock_guard<mutex> lock(mtx_task_config);
    id_descriptor_to_data[id_descriptor] = std::move(fd.clone() / norm_l2);
    id_descriptor_to_id_vstreams[id_descriptor].insert(id_vstream);
    id_vstream_to_id_descriptors[id_vstream].insert(id_descriptor);
  }

  return id_descriptor;
}

int Singleton::addLogFace(int id_vstream, const QDateTime& log_date, int id_descriptor, double quality,
   const cv::Rect& face_rect, const string& screenshot) const
{
  QString file_name = QCryptographicHash::hash((log_date.toString("yyyy-MM-dd_hh:mm:ss:zzz") +
    QString::number(id_vstream)).toUtf8(), QCryptographicHash::Md5).toHex().toLower();
  QString dir_path = QString("/%1/%2/%3").arg(file_name[0]).arg(file_name[1]).arg(file_name[2]);
  QString file_path = QString("%1/%2.jpg").arg(dir_path, file_name);

  //для теста
  //auto tt0 = std::chrono::steady_clock::now();

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());
  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    addLog(sql_db.lastError().text());
    return 0;
  }

  QSqlQuery q_add_log_face(sql_db);
  if (!q_add_log_face.prepare(SQL_ADD_LOG_FACE))
  {
    addLog("Ошибка: не удалось подготовить запрос SQL_ADD_LOG_FACE.");
    addLog(q_add_log_face.lastError().text());
    return 0;
  }

  q_add_log_face.bindValue(":id_vstream", id_vstream);
  q_add_log_face.bindValue(":log_date", log_date);
  if (id_descriptor > 0)
    q_add_log_face.bindValue(":id_descriptor", id_descriptor);
  else
    q_add_log_face.bindValue("id_descriptor", QVariant());
  q_add_log_face.bindValue(":quality", quality);
  q_add_log_face.bindValue(":face_left", face_rect.x);
  q_add_log_face.bindValue(":face_top", face_rect.y);
  q_add_log_face.bindValue(":face_width", face_rect.width);
  q_add_log_face.bindValue(":face_height", face_rect.height);
  q_add_log_face.bindValue(":screenshot", file_path);
  if (!q_add_log_face.exec())
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_ADD_LOG_FACE.");
    addLog(q_add_log_face.lastError().text());
    return 0;
  }

  int id_log = q_add_log_face.lastInsertId().toInt();

  if (!sql_transaction.commit())
  {
    addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    addLog(sql_transaction.getDatabase().lastError().text());
    return 0;
  }

  //для теста
  //auto tt1 = std::chrono::steady_clock::now();
  //cout << "__write log_face time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << " ms" << endl;

  //пишем файл со скриншотом
  QDir d;
  d.mkpath(screenshot_path + dir_path);
  QFile f(screenshot_path + file_path);
  f.open(QFile::WriteOnly);
  f.write(screenshot.data(), screenshot.size());
  f.close();

  return id_log;
}

//рекурсивная функция для удаления ненужных скриншотов
void removeFiles(const QString& path, const QDateTime& dt)
{
  QDir d(path);
  QFileInfoList fi_files = d.entryInfoList({"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.ppm", "*.tiff"}, QDir::Files);
  for (auto it = fi_files.begin(); it != fi_files.end(); ++it)
    if (it->lastModified() < dt)
      QFile::remove(it->absoluteFilePath());

  QFileInfoList fi_dirs = d.entryInfoList({"*"}, QDir::Dirs | QDir::NoDotAndDotDot);
  for (auto& fi_dir : fi_dirs)
    removeFiles(fi_dir.absoluteFilePath(), dt);
}

void Singleton::removeOldLogFaces()
{
  float interval_live;
  //scope for lock mutex
  {
    lock_guard<mutex> lock(mtx_task_config);
    interval_live = task_config.conf_params[CONF_LOG_FACES_LIVE_INTERVAL].value.toFloat();
  }

  QDateTime log_date = QDateTime::currentDateTime();
  log_date = log_date.addSecs(-interval_live * 3600);

  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());
  QSqlQuery q_remove_logs(sql_db);
  if (!q_remove_logs.prepare(SQL_REMOVE_OLD_LOG_FACES))
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_REMOVE_OLD_LOG_FACES.");
    addLog(q_remove_logs.lastError().text());
    return;
  }
  q_remove_logs.bindValue(":log_date", log_date);
  if (!q_remove_logs.exec())
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_REMOVE_OLD_LOG_FACES.");
    addLog(q_remove_logs.lastError().text());
    return;
  }

  //удаляем уже ненужные файлы со скриншотами
  removeFiles(screenshot_path, log_date);
}

void Singleton::addLogDeliveryEvent(DeliveryEventType delivery_type, DeliveryEventResult delivery_result, int id_vstream,
  int id_descriptor) const
{
  SQLGuard sql_guard;
  QSqlDatabase sql_db = QSqlDatabase::database(sql_guard.connectionName());
  SqlTransaction sql_transaction(sql_guard.connectionName());
  if (!sql_transaction.inTransaction())
  {
    addLog(QString("Ошибка: не удалось запустить транзакцию в функции %1.").arg(__FUNCTION__));
    addLog(sql_db.lastError().text());
    return;
  }

  QSqlQuery q_add_log_delivery_event(sql_db);
  if (!q_add_log_delivery_event.prepare(SQL_ADD_LOG_DELIVERY_EVENT))
  {
    addLog("Ошибка: не удалось подготовить запрос SQL_ADD_LOG_DELIVERY_EVENT.");
    addLog(q_add_log_delivery_event.lastError().text());
    return;
  }
  q_add_log_delivery_event.bindValue(":delivery_type", static_cast<int>(delivery_type));
  q_add_log_delivery_event.bindValue(":delivery_result", static_cast<int>(delivery_result));
  q_add_log_delivery_event.bindValue(":id_vstream", id_vstream);
  q_add_log_delivery_event.bindValue(":id_descriptor", id_descriptor);
  if (!q_add_log_delivery_event.exec())
  {
    addLog("Ошибка: не удалось выполнить запрос SQL_ADD_LOG_DELIVERY_EVENT.");
    addLog(q_add_log_delivery_event.lastError().text());
    return;
  }

  if (!sql_transaction.commit())
  {
    addLog(QString("Ошибка: не удалось завершить транзакцию в функции %1.").arg(__FUNCTION__));
    addLog(sql_transaction.getDatabase().lastError().text());
    return;
  }
}
