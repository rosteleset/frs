#include "sql_pool.h"
#include <utility>


SQLPool::SQLPool(int pool_size_, QString prefix_, QString  sql_driver_, QString  sql_options_,
  QString  sql_host_, int sql_port_, QString  db_name_, QString  user_name_, QString password_)
  : pool_size(pool_size_), prefix(std::move(prefix_)), sql_driver(std::move(sql_driver_)), sql_options(std::move(sql_options_)), sql_host(std::move(sql_host_)),
  sql_port(sql_port_), db_name(std::move(db_name_)), user_name(std::move(user_name_)), password(std::move(password_))
{
  for (int i = 0; i < pool_size; ++i)
  {
    QString connection_name = QString("%1_%2").arg(prefix).arg(i);
    free_connections.insert(connection_name);
  }
}

SQLPool::~SQLPool()
{
  //scope for lock mutext
  {
    std::lock_guard<std::mutex> lock(mtx_pool);
    is_working = false;
  }
  cv_pool.notify_all();
}

QString SQLPool::acquireConnection()
{
  QString conn_name;
  //scope for lock mutext
  {
    std::unique_lock<std::mutex> lock(mtx_pool);
    cv_pool.wait(lock, [this]
    {
      return !free_connections.empty() || !is_working;
    });

    if (!is_working)
      return "";

    conn_name = *free_connections.begin();
    free_connections.remove(conn_name);
  }

  QSqlDatabase sql_db = QSqlDatabase::addDatabase(sql_driver, conn_name);
  if (!sql_options.isEmpty())
    sql_db.setConnectOptions(sql_options);
  sql_db.setHostName(sql_host);
  sql_db.setPort(sql_port);
  sql_db.setDatabaseName(db_name);
  sql_db.setUserName(user_name);
  sql_db.setPassword(password);

  //для теста
  //std::cout << "__connection acquired: " << conn_name.toStdString() << "; pool free connections: " << free_connections.size() << std::endl;

  return conn_name;
}

void SQLPool::releaseConnection(const QString& connection_name)
{
  if (connection_name.isEmpty())
    return;

  //scope for correct destroying
  {
    QSqlDatabase sql_db = QSqlDatabase::database(connection_name);
    sql_db.close();
  }
  QSqlDatabase::removeDatabase(connection_name);

  //scope for lock mutex
  {
    std::lock_guard<std::mutex> lock(mtx_pool);
    free_connections.insert(connection_name);

    //для теста
    //std::cout << "__connection released: " << connection_name.toStdString() << "; pool free connections: " << free_connections.size() << std::endl;
  }
  cv_pool.notify_one();
}
