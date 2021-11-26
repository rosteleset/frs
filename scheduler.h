#pragma once

#include <iostream>
#include <chrono>
#include <queue>
#include <utility>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <csignal>

#include <Wt/Http/Client.h>
#include <Wt/Http/Message.h>
#include <Wt/Http/ResponseContinuation.h>
#include <QtCore>

#include <opencv2/opencv.hpp>

#include <boost/bind.hpp>

using namespace std;
using time_point = chrono::time_point<chrono::steady_clock>;

enum TaskType
{
  TASK_NONE,
  TASK_CAPTURE_VSTREAM_FRAME,
  TASK_GET_FRAME_FROM_URL,
  TASK_RECOGNIZE,
  TASK_REGISTER_DESCRIPTOR,
  TASK_REMOVE_OLD_LOG_FACES

  , TASK_TEST
};

enum DeliveryEventType
{
  FACE_RECOGNIZED = 1
};

enum DeliveryEventResult
{
  ERROR = 0,
  SUCCESSFUL = 1
};

enum FaceClassIndexes
{
  FACE_NONE = -1,
  FACE_NORMAL = 0,
};

//для теста
struct Counters
{
  int g_create_task_count{};
  int g_destroy_task_count{};
  int g_enqueue_task_count{};
  int g_dequeue_task_count{};
  int create_task_count{};
  int destroy_task_count{};
  int enqueue_task_count{};
  int dequeue_task_count{};
};

//для теста
extern Counters counters;
extern mutex mtx_counters;

struct TaskInfo;
struct TaskData
{
  int id_vstream{};
  TaskType task_type{TASK_NONE};
  bool permanent_task{};
  int error_count{};
  cv::Mat frame{};
  std::string frame_data;
  bool is_registration{};
  QString frame_url;
  Wt::Http::ResponseContinuation* response_continuation = nullptr;  //указатель на рабочий объект для api методов, которые не могут сразу же вернуть результат

  TaskData(int id_vstream_, TaskType task_type_, bool permanent_task_ = false, int error_count_ = 0, cv::Mat frame_ = cv::Mat());
  explicit TaskData(TaskType task_type_, cv::Mat frame_ = cv::Mat());
  explicit TaskData(const TaskInfo& ti);
  TaskData();

  ~TaskData()
  {
    //для теста
    //cout << "__destructor TaskData, task_type = " << task_type << ", camera: " << from_camera << endl;

    lock_guard<mutex> lock(mtx_counters);
    ++counters.destroy_task_count;
  }
};

struct TaskInfo
{
  int id_vstream{};
  TaskType task_type{TASK_NONE};
  bool permanent_task{};
  int error_count{};
  bool is_registration{};
  QString frame_url;
  Wt::Http::ResponseContinuation* response_continuation = nullptr;

  explicit TaskInfo(const TaskData& td)
    : id_vstream(td.id_vstream), task_type(td.task_type), permanent_task(td.permanent_task), error_count(td.error_count),
      is_registration(td.is_registration), frame_url(td.frame_url), response_continuation(td.response_continuation)
  {

  }
};

struct TaskConfig;

struct RegisterDescriptorResponse
{
  int id_descriptor = 0;
  QString comments = "";  //комментарии к выполнению запроса
  cv::Mat face_image;
  int face_left = 0;
  int face_top = 0;
  int face_width = 0;
  int face_height = 0;
};

class Scheduler
{
public:
  Scheduler(int pool_size, int log_level_);
  ~Scheduler();
  void addTask(const shared_ptr<TaskData>& task_data, double delayToRun);
  void addPermanentTasks();

  //для теста
  /*void printCounters()
  {
    //scope for lock mutex
    Counters qq;
    {
      lock_guard<mutex> lock(mtx_counters);
      qq = counters;
    }


    cout << "Global created tasks: " << qq.g_create_task_count << "\n";
    cout << "Global destroyed tasks: " << qq.g_destroy_task_count << "\n";
    cout << "Global enqueued tasks: " << qq.g_enqueue_task_count << "\n";
    cout << "Global dequeued tasks: " << qq.g_dequeue_task_count << "\n";
    cout << "Created tasks: " << qq.create_task_count << "\n";
    cout << "Destroyed tasks: " << qq.destroy_task_count << "\n";
    cout << "Enqueued tasks: " << qq.enqueue_task_count << "\n";
    cout << "Dequeued tasks: " << qq.dequeue_task_count << "\n";
    cout << "--------------------------\n";
  }*/

private:
  struct WorkerTaskData
  {
    //для теста
    WorkerTaskData(time_point _tp, shared_ptr<TaskData> _data) : tp(_tp), data(std::move(_data))
    {
      //cout << "__contructor WorkerTaskData, task type: " << data->task_type << endl;
      lock_guard<mutex> lock(mtx_counters);
      ++counters.g_create_task_count;
    }
    WorkerTaskData()
    {
      //cout << "__contructor WorkerTaskData, task type: " << data->task_type << endl;
      lock_guard<mutex> lock(mtx_counters);
      ++counters.g_create_task_count;
    }
    ~WorkerTaskData()
    {
      //cout << "__destructor WorkerTaskData, task type: " << data->task_type << endl;
      lock_guard<mutex> lock(mtx_counters);
      ++counters.g_destroy_task_count;
    }

    time_point tp;
    shared_ptr<TaskData> data;
  };

  struct WorkerTaskDataCompare
  {
    bool operator()(const shared_ptr<WorkerTaskData>& left, const shared_ptr<WorkerTaskData>& right)
    {
      //для теста
      //cout << "__compare\n";

      return left->tp > right->tp;
    }
  };

  int thread_pool_size;
  int log_level;

  bool is_scheduler_running = true;
  bool is_worker_running = true;
  mutex mtx_scheduler;
  mutex mtx_workers;
  condition_variable cv_scheduler;
  condition_variable cv_workers;
  thread t_scheduler;
  vector<thread> t_pool;
  priority_queue<shared_ptr<WorkerTaskData>, vector<shared_ptr<WorkerTaskData>>, WorkerTaskDataCompare> task_queue;
  queue<shared_ptr<TaskData>> tasks_ready_to_run;

  void schedulerThread();
  void workerThread();
  void runWorkerTask(const shared_ptr<TaskData>& task_data, TaskConfig& task_config);
  void handleHttpScreenshotResponse(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& response,
    Wt::Http::Client* client, TaskInfo task_info, int max_error_count, int retry_pause);
  void handleHttpDeliveryEventResponse(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& response,
    Wt::Http::Client* client, DeliveryEventType delivery_type, int id_vstream, int descriptor);
  void processFrame(TaskData* task_data, TaskConfig& task_config, RegisterDescriptorResponse* response);
};
