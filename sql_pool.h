#pragma once

#include <mutex>
#include <condition_variable>
#include <QtCore>
#include <QtSql>

class SQLPool
{
public:
  SQLPool(int pool_size_, QString  prefix_, QString  sql_driver_, QString  sql_options_,
    QString  sql_host_, int sql_port_, QString  db_name_, QString  user_name_, QString  password_);
  ~SQLPool();
  QString acquireConnection();
  void releaseConnection(const QString& connection_name);

private:
  int pool_size;
  QString prefix;
  QString sql_driver;
  QString sql_options;
  QString sql_host;
  int sql_port;
  QString db_name;
  QString user_name;
  QString password;
  QSet<QString> free_connections;
  bool is_working = true;
  std::mutex mtx_pool;
  std::condition_variable cv_pool;
};

