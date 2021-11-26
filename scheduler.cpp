#include <chrono>
#include <utility>

#include "scheduler.h"
#include "singleton.h"
#include "frs_api.h"
#include "http_client.h"
#include "sql_pool.h"

#include <Wt/WServer.h>
#include <Wt/WIOService.h>

Counters counters;
mutex mtx_counters;

namespace ni = nvidia::inferenceserver;
namespace nic = nvidia::inferenceserver::client;

namespace
{
  constexpr const int HTTP_SUCCESS = 200;
  constexpr const int HTTP_NO_CONTENT = 204;
}

// OpenCV port of 'LAPV' algorithm (Pech2000)
double varianceOfLaplacian(const cv::Mat& src)
{
  cv::Mat lap;
  cv::Laplacian(src, lap, CV_64F);
  int mrg = 3;  //обрезаем границу
  lap = lap(cv::Rect(mrg, mrg, lap.cols - 2 * mrg, lap.rows - 2 * mrg));

  cv::Scalar mu, sigma;
  cv::meanStdDev(lap, mu, sigma);

  double focusMeasure = sigma.val[0] * sigma.val[0];
  return focusMeasure;
}

double dist(int x1, int y1, int x2, int y2)
{
  return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

bool isFrontalFace(const cv::Mat& landmarks)
{
  if (landmarks.empty())
    return false;

  //нос должен быть между глазами
  float nx = landmarks.at<float>(2, 0);
  float ny = landmarks.at<float>(2, 1);
  if (nx <= landmarks.at<float>(0, 0) || nx >= landmarks.at<float>(1, 0)
    || ny <= landmarks.at<float>(0, 1) || ny <= landmarks.at<float>(1, 1))
    return false;

  //правый глаз не должен быть правее левого кончика губ
  //левый глаз не должен быть левее правого кончика губ
  if (landmarks.at<float>(0, 0) >= landmarks.at<float>(4, 0)
    || landmarks.at<float>(1, 0) <= landmarks.at<float>(3, 0))
    return false;
  bool is_frontal = true;
  double equal_threshold = 0.62;  //порог равенства двух длин

  //расстояние от правого глаза до носа
  double d1 = dist(landmarks.at<float>(0, 0), landmarks.at<float>(0, 1), nx, ny);

  //расстояние от левого глаза до носа
  double d2 = dist(landmarks.at<float>(1, 0), landmarks.at<float>(1, 1), nx, ny);
  is_frontal = is_frontal && (std::min(d1, d2) / std::max(d1, d2) > equal_threshold);

  //расстояние от правого кончика губ до носа
  d1 = dist(landmarks.at<float>(3, 0), landmarks.at<float>(3, 1), nx, ny);

  //расстояние от левого кончика губ до носа
  d2 = dist(landmarks.at<float>(4, 0), landmarks.at<float>(4, 1), nx, ny);
  is_frontal = is_frontal && (std::min(d1, d2) / std::max(d1, d2) > equal_threshold);

  //расстояние от правого кончика губ до правого глаза
  d1 = dist(landmarks.at<float>(3, 0), landmarks.at<float>(3, 1), landmarks.at<float>(0, 0), landmarks.at<float>(0, 1));

  //расстояние от левого кончика губ до левого глаза
  d2 = dist(landmarks.at<float>(4, 0), landmarks.at<float>(4, 1), landmarks.at<float>(1, 0), landmarks.at<float>(1, 1));
  is_frontal = is_frontal && (std::min(d1, d2) / std::max(d1, d2) > equal_threshold);

  //расстояние между глазами
  d1 = dist(landmarks.at<float>(0, 0), landmarks.at<float>(0, 1), landmarks.at<float>(1, 0), landmarks.at<float>(1, 1));

  //расстояние между кончиками губ
  d2 = dist(landmarks.at<float>(3, 0), landmarks.at<float>(3, 1), landmarks.at<float>(4, 0), landmarks.at<float>(4, 1));

  //расстояние между правым глазом и правым кончиком губ
  double d3 = dist(landmarks.at<float>(0, 0), landmarks.at<float>(0, 1), landmarks.at<float>(3, 0), landmarks.at<float>(3, 1));

  //расстояние между левым глазом и левым кончиком губ
  double d4 = dist(landmarks.at<float>(1, 0), landmarks.at<float>(1, 1), landmarks.at<float>(4, 0), landmarks.at<float>(4, 1));

  d1 = std::max(d1, d2);
  d2 = std::max(d3, d4);
  is_frontal = is_frontal && (std::min(d1, d2) / std::max(d1, d2) > equal_threshold);

  return is_frontal;
}

HeadPoseEstimation getHeadPose(const cv::Mat& landmarks5, const cv::Rect& f_rect)
{
  HeadPoseEstimation hp;
  if (landmarks5.rows != 5 && landmarks5.cols != 2)
    return hp;

  vector<cv::Point3f> face_points_world = {

    /*{38.2946f, 51.6963f, 0.0f},
    {73.5318f, 51.5014f, 0.0f},
    {56.0252f, 71.7366f, -5.0f},
    {41.5493f, 92.3655f, 0.0f},
    {70.7299f, 92.2041f, 0.0f},*/

    {-200.0f, -170.0f, 135.0f}, // правый глаз
    {200.0f, -170.0f, 135.0f},  // левый глаз
    {0.0f, 0.0f, 0.0f},         // нос
    {-150.0f, 150.0f, 125.0f},  // правый угол рта
    {150.0f, 150.0f, 125.0f}    // левый угол рта

    /*{-165.0f, -170.0f, 135.0f}, // правый глаз
    {165.0f, -170.0f, 135.0f},  // левый глаз
    {0.0f, 0.0f, 0.0f},         // нос
    {-150.0f, 150.0f, 125.0f},  // правый угол рта
    {150.0f, 150.0f, 125.0f}    // левый угол рта*/
  };

  vector<cv::Point2f> face_points_camera = {
    {landmarks5.at<float>(0, 0), landmarks5.at<float>(0, 1)},  // правый глаз
    {landmarks5.at<float>(1, 0), landmarks5.at<float>(1, 1)},  // левый глаз
    {landmarks5.at<float>(2, 0), landmarks5.at<float>(2, 1)},  // нос
    {landmarks5.at<float>(3, 0), landmarks5.at<float>(3, 1)},  // правый угол рта
    {landmarks5.at<float>(4, 0), landmarks5.at<float>(4, 1)}   // левый угол рта

    /*{38.2946f, 51.6963f},
    {73.5318f, 51.5014f},
    {56.0252f, 71.7366f},
    {41.5493f, 92.3655f},
    {70.7299f, 92.2041f},*/

  };

  float cx = f_rect.tl().x + f_rect.width / 2.0f;
  float cy = f_rect.tl().y + f_rect.height / 2.0f;
  float focal_length = max(f_rect.width, f_rect.height);
  cv::Mat camera_matrix = (cv::Mat_<float>(3, 3) <<
     focal_length, 0.0f, cx,
     0.0f, focal_length, cy,
     0.0f, 0.0f, 1.0f);
  cv::Mat rotation_vector;
  cv::Mat translation_vector;
  bool is_ok = cv::solvePnP(face_points_world, face_points_camera, camera_matrix, cv::noArray(), rotation_vector,
    translation_vector, false, cv::SOLVEPNP_EPNP);
  if (is_ok)
  {
    /*rotation_vector.at<double>(0, 0) = 0.5f;
    rotation_vector.at<double>(1, 0) = 0.001f;
    rotation_vector.at<double>(2, 0) = 0.0f;*/
    //cout << "Rotation vector: " << rotation_vector << endl;
    vector<cv::Point3f> axis = {
      {0.0f, 0.0f, 0.0f},
      {100.0f, 0.0f, 0.0f},
      {0.0f, 100.0f, 0.0f},
      {0.0f, 0.0f, 100.0f}
    };
    cv::projectPoints(axis, rotation_vector, translation_vector, camera_matrix, cv::noArray(), hp.axis);

    cv::Mat rvec_matrix;
    cv::Rodrigues(rotation_vector, rvec_matrix);
    cv::Mat proj_matrix;
    cv::hconcat(rvec_matrix, translation_vector, proj_matrix);
    cv::Mat euler_angles;
    cv::Mat m1, m2, m3, m4, m5, m6;
    cv::decomposeProjectionMatrix(proj_matrix, m1, m2,
      m3, m4, m5, m6, euler_angles);

    hp.angle_p = euler_angles.at<double>(0, 0);
    hp.angle_y = euler_angles.at<double>(0, 1);
    hp.angle_r = euler_angles.at<double>(0, 2);
  }

  return hp;
}

struct FaceClass
{
  int class_index;
  float score;
};

struct alignas(float) FaceDetection
{
  float bbox[4];  //x1 y1 x2 y2
  float face_confidence;
  float landmark[10];
};

bool cmp(const FaceDetection& a, const FaceDetection& b)
{
  return a.face_confidence > b.face_confidence;
}

cv::Mat preprocessImage(const cv::Mat& img, int width, int height, float& scale)
{
  int w, h;
  float r_w = width / (img.cols * 1.0);
  float r_h = height / (img.rows * 1.0);
  if (r_h > r_w) {
    w = width;
    h = r_w * img.rows;
  } else
  {
    w = r_h * img.cols;
    h = height;
  }
  cv::Mat re(h, w, CV_8UC3);
  cv::resize(img, re, re.size(), 0, 0, cv::INTER_LINEAR);
  cv::Mat out(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  re.copyTo(out(cv::Rect(0, 0, re.cols, re.rows)));
  scale = float(h) / img.rows;

  return out;
}

//intersection over union
float iou(float lbox[4], float rbox[4])
{
  float interBox[] =
  {
    std::max(lbox[0], rbox[0]), //left
    std::min(lbox[2], rbox[2]), //right
    std::max(lbox[1], rbox[1]), //top
    std::min(lbox[3], rbox[3]), //bottom
  };

  if(interBox[2] > interBox[3] || interBox[0] > interBox[1])
    return 0.0f;

  float interBoxS = (interBox[1] - interBox[0]) * (interBox[3] - interBox[2]);
  return interBoxS / ((lbox[2] - lbox[0]) * (lbox[3] - lbox[1]) + (rbox[2] - rbox[0]) * (rbox[3] - rbox[1]) - interBoxS + 0.000001f);
}

//Алгоритм non maximum suppression
static inline void nms(std::vector<FaceDetection>& dets, float nms_thresh = 0.4)
{
  std::sort(dets.begin(), dets.end(), cmp);
  for (size_t m = 0; m < dets.size(); ++m)
  {
    auto& item = dets[m];
    for (size_t n = m + 1; n < dets.size(); ++n)
    {
      if (iou(item.bbox, dets[n].bbox) > nms_thresh)
      {
        dets.erase(dets.begin() + n);
        --n;
      }
    }
  }
}

//выравнивание области детекции лица
cv::Mat alignFaceAffineTransform(const cv::Mat& frame, const cv::Mat& src, int face_width, int face_height)
{
  cv::Mat dst = (cv::Mat_<double>(5, 2) <<
                 38.2946 / 112.0 * face_width, 51.6963 / 112.0 * face_height,
                 73.5318 / 112.0 * face_width, 51.5014 / 112.0 * face_height,
                 56.0252 / 112.0 * face_width, 71.7366 / 112.0 * face_height,
                 41.5493 / 112.0 * face_width, 92.3655 / 112.0 * face_height,
                 70.7299 / 112.0 * face_width, 92.2041 / 112.0 * face_height);
  cv::Mat inliers;
  cv::Mat mm = cv::estimateAffinePartial2D(src, dst, inliers, cv::LMEDS);
  cv::Mat r;
  cv::warpAffine(frame, r, mm, cv::Size(face_width, face_height));
  return r;
}

//поиск лиц в кадре
bool detectFaces(int id_vstream, const cv::Mat& frame, const TaskConfig& task_config, vector<FaceDetection>& detected_faces)
{
  Singleton& singleton = Singleton::instance();

  string server_url = task_config.vstream_conf_params[id_vstream][CONF_DNN_FD_INFERENCE_SERVER].value.toString().toStdString();
  string model_name = task_config.conf_params[CONF_DNN_FD_MODEL_NAME].value.toString().toStdString();
  string input_tensor_name = task_config.conf_params[CONF_DNN_FD_INPUT_TENSOR_NAME].value.toString().toStdString();
  int input_width = task_config.conf_params[CONF_DNN_FD_INPUT_WIDTH].value.toInt();
  int input_height = task_config.conf_params[CONF_DNN_FD_INPUT_HEIGHT].value.toInt();
  float face_confidence_threshold = task_config.vstream_conf_params[id_vstream][CONF_FACE_CONFIDENCE_THRESHOLD].value.toFloat();

  unique_ptr<nic::InferenceServerHttpClient> triton_client;
  auto err = nic::InferenceServerHttpClient::Create(&triton_client, server_url, false);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать клиента для инференса: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }

  //для теста
  //auto tt1 = std::chrono::steady_clock::now();
  //cout << "  before preprocess time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;
  //tt0 = tt1;

  float scale = 1.0f;
  cv::Mat pr_img = preprocessImage(frame, input_width, input_height, scale);

  //tt1 = std::chrono::steady_clock::now();
  //cout << "  preprocess image time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;
  //tt0 = tt1;

  int channels = 3;
  int input_size = channels * input_width * input_height;
  vector<float> input_buffer(input_size);

  for (size_t c = 0; c < channels; ++c)
    for (size_t h = 0; h < input_height; ++h)
      for (size_t w = 0; w < input_width; ++w)
        input_buffer[c * input_height * input_width + h * input_width + w] = (pr_img.at<cv::Vec3b>(h, w)[2 - c] - 127.5f) / 128.0;

  vector<uint8_t> input_data(input_size * sizeof(float));
  memcpy(input_data.data(), input_buffer.data(), input_data.size());
  std::vector<int64_t> shape = {1, channels, input_height, input_width};
  nic::InferInput* input;
  err = nic::InferInput::Create(&input, input_tensor_name, shape, "FP32");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать входные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferInput> input_ptr(input);

  nic::InferRequestedOutput* output497;
  err = nic::InferRequestedOutput::Create(&output497, "497");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output497_ptr(output497);

  nic::InferRequestedOutput* output494;
  err = nic::InferRequestedOutput::Create(&output494, "494");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output494_ptr(output494);

  nic::InferRequestedOutput* output477;
  err = nic::InferRequestedOutput::Create(&output477, "477");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output477_ptr(output477);

  nic::InferRequestedOutput* output454;
  err = nic::InferRequestedOutput::Create(&output454, "454");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output454_ptr(output454);

  nic::InferRequestedOutput* output451;
  err = nic::InferRequestedOutput::Create(&output451, "451");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output451_ptr(output451);

  nic::InferRequestedOutput* output474;
  err = nic::InferRequestedOutput::Create(&output474, "474");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output474_ptr(output474);

  nic::InferRequestedOutput* output448;
  err = nic::InferRequestedOutput::Create(&output448, "448");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output448_ptr(output448);

  nic::InferRequestedOutput* output500;
  err = nic::InferRequestedOutput::Create(&output500, "500");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output500_ptr(output500);

  nic::InferRequestedOutput* output471;
  err = nic::InferRequestedOutput::Create(&output471, "471");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output471_ptr(output471);

  std::vector<nic::InferInput*> inputs = {input_ptr.get()};
  std::vector<const nic::InferRequestedOutput*> outputs = {
      output497_ptr.get(), output494_ptr.get(), output477_ptr.get(), output454_ptr.get(), output451_ptr.get()
    , output474_ptr.get(), output448_ptr.get(), output500_ptr.get(), output471_ptr.get()
  };
  err = input_ptr->AppendRaw(input_data);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось установить входные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }

  nic::InferOptions options(model_name);
  options.model_version_ = "";
  nic::InferResult* result;

  //для теста
  //tt1 = std::chrono::steady_clock::now();
  //cout << "  before inference time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;
  //tt0 = tt1;

  err = triton_client->Infer(&result, options, inputs, outputs);

  //tt1 = std::chrono::steady_clock::now();
  //cout << "  inference time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;
  //tt0 = tt1;

  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось отправить синхронный запрос инференса: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferResult> result_ptr(result);

  if (!result_ptr->RequestStatus().IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось получить результат инференса: %1").arg(
      QString::fromStdString(result_ptr->RequestStatus().Message())));
    return false;
  }

  vector<string> output_names = {"448", "471", "494", "451", "474", "497", "454", "477", "500"};
  vector<int> feat_stride = {8, 16, 32};
  int fmc = 3;

  detected_faces.clear();
  for (int i = 0; i < feat_stride.size(); ++i)
  {
    float* scores_data;
    size_t scores_size;
    result_ptr->RawData(output_names[i], (const uint8_t**)(&scores_data), &scores_size);

    float* bbox_preds_data;
    size_t bbox_preds_size;
    result_ptr->RawData(output_names[i + fmc], (const uint8_t**)(&bbox_preds_data), &bbox_preds_size);
    auto bbox_preds = cv::Mat(bbox_preds_size / 4 / sizeof(float), 4, CV_32F, bbox_preds_data);
    bbox_preds *= feat_stride[i];

    float* kps_preds_data;
    size_t kps_preds_size;
    result_ptr->RawData(output_names[i + fmc * 2], (const uint8_t**)(&kps_preds_data), &kps_preds_size);
    auto kps_preds = cv::Mat(kps_preds_size / 10 / sizeof(float), 10, CV_32F, kps_preds_data);
    kps_preds *= feat_stride[i];

    int height = input_height / feat_stride[i];
    int width = input_width / feat_stride[i];
    for (int k = 0; k < height * width; ++k)
    {
      float px = feat_stride[i] * (k % height);
      float py = feat_stride[i] * (k / height);
      if (scores_data[2 * k] >= face_confidence_threshold)
      {
        FaceDetection det{};
        det.face_confidence = scores_data[2 * k];
        det.bbox[0] = (px - bbox_preds.at<float>(2 * k, 0)) / scale;
        det.bbox[1] = (py - bbox_preds.at<float>(2 * k, 1)) / scale;
        det.bbox[2] = (px + bbox_preds.at<float>(2 * k, 2)) / scale;
        det.bbox[3] = (py + bbox_preds.at<float>(2 * k, 3)) / scale;
        for (int j = 0; j < 5; ++j)
        {
          det.landmark[2 * j] = (px + kps_preds.at<float>(2 * k, 2 * j)) / scale;
          det.landmark[2 * j + 1] = (py + kps_preds.at<float>(2 * k, 2 * j + 1)) / scale;
        }
        detected_faces.push_back(det);
      }
      if (scores_data[2 * k + 1] >= face_confidence_threshold)
      {
        FaceDetection det{};
        det.face_confidence = scores_data[2 * k + 1];
        det.bbox[0] = (px - bbox_preds.at<float>(2 * k + 1, 0)) / scale;
        det.bbox[1] = (py - bbox_preds.at<float>(2 * k + 1, 1)) / scale;
        det.bbox[2] = (px + bbox_preds.at<float>(2 * k + 1, 2)) / scale;
        det.bbox[3] = (py + bbox_preds.at<float>(2 * k + 1, 3)) / scale;
        for (int j = 0; j < 5; ++j)
        {
          det.landmark[2 * j] = (px + kps_preds.at<float>(2 * k + 1, 2 * j)) / scale;
          det.landmark[2 * j + 1] = (py + kps_preds.at<float>(2 * k + 1, 2 * j + 1)) / scale;
        }
        detected_faces.push_back(det);
      }
    }
  }

  //для теста
  //tt1 = std::chrono::steady_clock::now();
  //cout << "  post process time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;

  nms(detected_faces);

  //для теста
  /*cout << "pos_bboxes:\n";
  for (int k = 0; k < detected_faces.size(); ++k)
  {
    for (int i = 0; i < 4; ++i)
      cout << detected_faces[k].bbox[i] << "  ";
    cout << endl;
    for (int i = 0; i < 5; ++i)
      cout << "  (" << detected_faces[k].landmark[2 * i] << ", " << detected_faces[k].landmark[2 * i + 1] << ")";
    cout << "  score: " << detected_faces[k].class_confidence << endl;

    auto landmarks5 = cv::Mat(5, 2, CV_32F, detected_faces[k].landmark);
    for (int i = 0; i < 5; ++i)
      cv::circle(frame, cv::Point(landmarks5.at<float>(i, 0), landmarks5.at<float>(i, 1)), 1, cv::Scalar(255 * (i * 2 > 2), 255 * (i * 2 > 0 && i * 2 < 8), 255 * (i * 2 < 6)), 4);
    cv::Rect face_rect = cv::Rect(detected_faces[k].bbox[0], detected_faces[k].bbox[1],
      detected_faces[k].bbox[2] - detected_faces[k].bbox[0] + 1, detected_faces[k].bbox[3] - detected_faces[k].bbox[1] + 1);
    cv::rectangle(frame, face_rect, cv::Scalar(0, 200, 0));
    cv::imwrite(singleton.working_path.toStdString() + "/t1_out.jpg", frame);
  }*/

  return true;
}

std::vector<FaceClass> softMax(const std::vector<float>& v)
{
   std::vector<FaceClass> r(v.size());
   float s = 0.0f;
   for (float i : v)
     s += exp(i);
   for (int i = 0; i < v.size(); ++i)
   {
     r[i].class_index = i;
     r[i].score = exp(v[i]) / s;
   }
   std::sort(r.begin(), r.end(), [](const auto& left, const auto& right) {return left.score > right.score;});
   return r;
}

bool inferFaceClass(int id_vstream, const cv::Mat& aligned_face, const TaskConfig& task_config, std::vector<FaceClass>& face_classes)
{
  Singleton& singleton = Singleton::instance();

  string server_url = task_config.vstream_conf_params[id_vstream][CONF_DNN_FC_INFERENCE_SERVER].value.toString().toStdString();
  string model_name = task_config.conf_params[CONF_DNN_FC_MODEL_NAME].value.toString().toStdString();
  string input_tensor_name = task_config.conf_params[CONF_DNN_FC_INPUT_TENSOR_NAME].value.toString().toStdString();
  int input_width = task_config.conf_params[CONF_DNN_FC_INPUT_WIDTH].value.toInt();
  int input_height = task_config.conf_params[CONF_DNN_FC_INPUT_HEIGHT].value.toInt();
  string output_tensor_name = task_config.conf_params[CONF_DNN_FC_OUTPUT_TENSOR_NAME].value.toString().toStdString();
  int class_count = task_config.conf_params[CONF_DNN_FC_OUTPUT_SIZE].value.toInt();

  unique_ptr<nic::InferenceServerHttpClient> triton_client;
  auto err = nic::InferenceServerHttpClient::Create(&triton_client, server_url, false);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать клиента для инференса: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }

  int channels = 3;
  int input_size = channels * input_width * input_height;
  vector<float> input_buffer(input_size);

  for (size_t c = 0; c < channels; ++c)
    for (size_t h = 0; h < input_height; ++h)
      for (size_t w = 0; w < input_width; ++w)
      {
        float mean = 0.0f;
        float std_d = 1.0f;
        if (c == 0)
        {
          mean = 0.485;
          std_d = 0.229;
        }
        if (c == 1)
        {
          mean = 0.456;
          std_d = 0.224;
        }
        if (c == 2)
        {
          mean = 0.406;
          std_d = 0.225;
        }
        input_buffer[c * input_height * input_width + h * input_width + w] = (aligned_face.at<cv::Vec3b>(h, w)[2 - c] / 255.0f - mean) / std_d;
      }

  vector<uint8_t> input_data(input_size * sizeof(float));
  memcpy(input_data.data(), input_buffer.data(), input_data.size());
  std::vector<int64_t> shape = {1, channels, input_height, input_width};
  nic::InferInput* input;
  err = nic::InferInput::Create(&input, input_tensor_name, shape, "FP32");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать входные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferInput> input_ptr(input);

  nic::InferRequestedOutput* output;
  err = nic::InferRequestedOutput::Create(&output, output_tensor_name);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output_ptr(output);
  std::vector<nic::InferInput*> inputs = {input_ptr.get()};
  std::vector<const nic::InferRequestedOutput*> outputs = {output_ptr.get()};
  err = input_ptr->AppendRaw(input_data);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось установить входные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }

  nic::InferOptions options(model_name);
  options.model_version_ = "";
  nic::InferResult* result;

  //для теста
  //auto tt0 = std::chrono::steady_clock::now();

  err = triton_client->Infer(&result, options, inputs, outputs);

  //для теста
  //auto tt1 = std::chrono::steady_clock::now();
  //cout << "  inference time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;

  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось отправить синхронный запрос инференса: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferResult> result_ptr(result);

  if (!result_ptr->RequestStatus().IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось получить результат инференса: %1").arg(
      QString::fromStdString(result_ptr->RequestStatus().Message())));
    return false;
  }

  shape.clear();
  err = result_ptr->Shape(output_tensor_name, &shape);
  if (!err.IsOk())
  {
    singleton.addLog("Ошибка! Не удалось получить размерность выходных данных.");
    return false;
  }

  std::string datatype;
  err = result_ptr->Datatype(output_tensor_name, &datatype);
  if (!err.IsOk())
  {
    singleton.addLog("Ошибка! Не удалось получить тип выходных данных.");
    return false;
  }

  float* result_data;
  size_t output_size;
  err = result_ptr->RawData(output_tensor_name, (const uint8_t**)(&result_data), &output_size);
  if (!err.IsOk())
  {
    singleton.addLog("Ошибка! Не удалось получить выходные данные.");
    return false;
  }

  std::vector<float> scores;
  scores.assign(result_data, result_data + class_count);
  face_classes = softMax(scores);

  return true;
}

bool extractFaceDescriptor(int id_vstream, const cv::Mat& aligned_face, const TaskConfig& task_config, FaceDescriptor& face_descriptor)
{
  Singleton& singleton = Singleton::instance();

  string server_url = task_config.vstream_conf_params[id_vstream][CONF_DNN_FR_INFERENCE_SERVER].value.toString().toStdString();
  string model_name = task_config.conf_params[CONF_DNN_FR_MODEL_NAME].value.toString().toStdString();
  string input_tensor_name = task_config.conf_params[CONF_DNN_FR_INPUT_TENSOR_NAME].value.toString().toStdString();
  int input_width = task_config.conf_params[CONF_DNN_FR_INPUT_WIDTH].value.toInt();
  int input_height = task_config.conf_params[CONF_DNN_FR_INPUT_HEIGHT].value.toInt();
  string output_tensor_name = task_config.conf_params[CONF_DNN_FR_OUTPUT_TENSOR_NAME].value.toString().toStdString();

  unique_ptr<nic::InferenceServerHttpClient> triton_client;
  auto err = nic::InferenceServerHttpClient::Create(&triton_client, server_url, false);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать клиента для инференса: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }

  int channels = 3;
  int input_size = channels * input_width * input_height;
  vector<float> input_buffer(input_size);
  for (size_t c = 0; c < channels; ++c)
    for (size_t h = 0; h < input_height; ++h)
      for (size_t w = 0; w < input_width; ++w)
        if (model_name == "arcface")
          input_buffer[c * input_height * input_width + h * input_width + w] = aligned_face.at<cv::Vec3b>(h, w)[2 - c] / 127.5f - 1.0f;
        else
          input_buffer[c * input_height * input_width + h * input_width + w] = (aligned_face.at<cv::Vec3b>(h, w)[2 - c] - 127.5f) / 128.0f;
  vector<uint8_t> input_data(input_size * sizeof(float));
  memcpy(input_data.data(), input_buffer.data(), input_data.size());
  std::vector<int64_t> shape = {1, channels, input_height, input_width};
  nic::InferInput* input;
  err = nic::InferInput::Create(&input, input_tensor_name, shape, "FP32");
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать входные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferInput> input_ptr(input);

  nic::InferRequestedOutput* output;
  err = nic::InferRequestedOutput::Create(&output, output_tensor_name);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось создать выходные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferRequestedOutput> output_ptr(output);
  std::vector<nic::InferInput*> inputs = {input_ptr.get()};
  std::vector<const nic::InferRequestedOutput*> outputs = {output_ptr.get()};
  err = input_ptr->AppendRaw(input_data);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось установить входные данные: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }

  nic::InferOptions options(model_name);
  options.model_version_ = "";
  nic::InferResult* result;
  err = triton_client->Infer(&result, options, inputs, outputs);
  if (!err.IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось отправить синхронный запрос инференса: %1").arg(QString::fromStdString(err.Message())));
    return false;
  }
  std::shared_ptr<nic::InferResult> result_ptr(result);

  if (!result_ptr->RequestStatus().IsOk())
  {
    singleton.addLog(QString("Ошибка! Не удалось получить результат инференса: %1").arg(
      QString::fromStdString(result_ptr->RequestStatus().Message())));
    return false;
  }

  shape.clear();
  err = result_ptr->Shape(output_tensor_name, &shape);
  if (!err.IsOk())
  {
    singleton.addLog("Ошибка! Не удалось получить размерность выходных данных.");
    return false;
  }

  std::string datatype;
  err = result_ptr->Datatype(output_tensor_name, &datatype);
  if (!err.IsOk())
  {
    singleton.addLog("Ошибка! Не удалось получить тип выходных данных.");
    return false;
  }

  float* result_data;
  size_t output_size;
  err = result_ptr->RawData(output_tensor_name, (const uint8_t**)(&result_data), &output_size);
  if (!err.IsOk())
  {
    singleton.addLog("Ошибка! Не удалось получить выходные данные.");
    return false;
  }

  auto m_result = cv::Mat(1, int(singleton.descriptor_size), CV_32F, result_data);
  face_descriptor = m_result.clone();

  return true;
}

cv::Rect enlargeFaceRect(const cv::Rect& face_rect, double k)
{
  int cx = face_rect.tl().x + face_rect.width / 2;
  int cy = face_rect.tl().y + face_rect.height / 2;
  int max_side = int(max(face_rect.width, face_rect.height) * k);
  return {cx - max_side / 2, cy - max_side / 2, max_side, max_side};
}

struct FaceData
{
  cv::Rect face_rect;
  bool is_work_area = false;
  bool is_frontal = false;
  bool is_non_blurry = false;
  FaceClassIndexes face_class_index = FaceClassIndexes::FACE_NONE;
  float face_class_confidence = 0.0f;
  double cosine_distance = -2.0;
  FaceDescriptor fd;
  cv::Mat landmarks5;
  double laplacian = 0.0;
  double ioa = 0.0;
  int id_descriptor = 0;
};

//TaskData
TaskData::TaskData(int id_vstream_, TaskType task_type_, bool permanent_task_, int error_count_, cv::Mat frame_)
  : id_vstream(id_vstream_), task_type(task_type_), permanent_task(permanent_task_), error_count(error_count_), frame(std::move(frame_))
{
  //для теста
  //cout << "__contructor TaskData, task type: " << task_type << endl;

  lock_guard<mutex> lock(mtx_counters);
  ++counters.create_task_count;
}

TaskData::TaskData(TaskType task_type_, cv::Mat frame_)
  : task_type(task_type_), frame(std::move(frame_))
{
  //для теста
  //cout << "__contructor TaskData, task type: " << task_type << endl;

  lock_guard<mutex> lock(mtx_counters);
  ++counters.create_task_count;
}

TaskData::TaskData(const TaskInfo& ti)
  : id_vstream(ti.id_vstream), task_type(ti.task_type), permanent_task(ti.permanent_task), error_count(ti.error_count),
    is_registration(ti.is_registration), frame_url(ti.frame_url), response_continuation(ti.response_continuation)
{
  //для теста
  //cout << "__contructor TaskData__" << endl;

  lock_guard<mutex> lock(mtx_counters);
  ++counters.create_task_count;
}

TaskData::TaskData()
{
  //для теста
  //cout << "__contructor TaskData__" << endl;

  lock_guard<mutex> lock(mtx_counters);
  ++counters.create_task_count;
}

//Scheduler public
Scheduler::Scheduler(int pool_size, int log_level_)
{
  thread_pool_size = pool_size;
  log_level = log_level_;
  t_scheduler = thread(&Scheduler::schedulerThread, this);
  for (int i = 0; i < thread_pool_size; ++i)
    t_pool.emplace_back(thread(&Scheduler::workerThread, this));
  if (log_level >= LOGS_EVENTS)
    Singleton::instance().addLog("Планировщик заданий запущен.");
}

Scheduler::~Scheduler()
{
  {
    lock_guard<mutex> lock(mtx_scheduler);
    is_scheduler_running = false;
  }
  cv_scheduler.notify_one();
  if (t_scheduler.joinable())
    t_scheduler.join();

  //scope for lock mutex
  {
    lock_guard<mutex> lock(mtx_workers);
    is_worker_running = false;
  }
  cv_workers.notify_all();
  for (auto&& t : t_pool)
    if (t.joinable())
      t.join();

  if (log_level >= LOGS_EVENTS)
    Singleton::instance().addLog("Планировщик заданий остановлен.");
}

void Scheduler::addTask(const shared_ptr<TaskData>& task_data, double delayToRun)
{
  if (task_data == nullptr)
    return;

  if (task_data == nullptr)
    return;

  if (delayToRun == 0.0)
  {
    {
      lock_guard<mutex> lock(mtx_workers);
      if (!is_worker_running)
        return;

      tasks_ready_to_run.push(task_data);
      if (tasks_ready_to_run.size() > 100)
        Singleton::instance().addLog("Слишком много заданий в очереди.");

      //для теста
      {
        lock_guard<mutex> c_locker(mtx_counters);
        ++counters.enqueue_task_count;
      }
    }
    if (log_level >= LOGS_VERBOSE)
      Singleton::instance().addLog(QString("Добавлено задание %1 (permanent = %2) в пул потоков для камеры id=%3").arg(
        task_data->task_type).arg(task_data->permanent_task).arg(task_data->id_vstream));
    cv_workers.notify_one();
  } else
  {
    auto now = chrono::steady_clock::now();
    {
      lock_guard<mutex> lock(mtx_scheduler);
      if (!is_scheduler_running)
        return;

      //task_queue.push({now + chrono::milliseconds(int(1000 * delayToRun)), task_data});
      task_queue.push(std::make_shared<WorkerTaskData>(now + chrono::milliseconds(int(1000 * delayToRun)), task_data));
    }

    //для теста
    {
      lock_guard<mutex> c_locker(mtx_counters);
      ++counters.g_enqueue_task_count;
    }

    if (log_level >= LOGS_VERBOSE)
      Singleton::instance().addLog(QString("Добавлено задание %1 (permanent = %2) планировщику для камеры id=%3").arg(
        task_data->task_type).arg(task_data->permanent_task).arg(task_data->id_vstream));
    cv_scheduler.notify_one();
  }
}

void Scheduler::addPermanentTasks()
{
  Singleton& singleton = Singleton::instance();
  TaskConfig task_config;
  //scope for lock mutex
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    task_config = singleton.task_config;
  }

  //регулярная очистка логов из таблицы log_faces
  auto clear_log_faces_task = std::make_shared<TaskData>(0, TASK_REMOVE_OLD_LOG_FACES, true);
  addTask(clear_log_faces_task, 0.0);
}

//Scheduler private
void Scheduler::schedulerThread()
{
  for (;;)
  {
    unique_lock lock{mtx_scheduler};

    if (!is_scheduler_running)
      break;

    bool check_tasks = true;
    auto wait_duration = chrono::nanoseconds{1000000000000};
    while (check_tasks) {
      if (!task_queue.empty())
      {
        auto now = chrono::steady_clock::now();
        if (now >= task_queue.top()->tp)
        {
          {
            lock_guard<mutex> lock2(mtx_workers);
            if (log_level >= LOGS_VERBOSE)
              Singleton::instance().addLog(QString("Добавлено задание в пул потоков для видео потока: %1 [%2]").arg(
                task_queue.top()->data->id_vstream).arg(
                std::chrono::duration_cast<std::chrono::milliseconds>(task_queue.top()->tp.time_since_epoch()).count()));
            tasks_ready_to_run.push(task_queue.top()->data);
          }
          task_queue.pop();

          //для теста
          {
            lock_guard<mutex> c_locker(mtx_counters);
            ++counters.enqueue_task_count;
            ++counters.g_dequeue_task_count;
          }

          cv_workers.notify_one();
        } else
        {
          check_tasks = false;
          wait_duration = task_queue.top()->tp - now;
        }
      } else
        check_tasks = false;
    }

    cv_scheduler.wait_for(lock, wait_duration);
  }
}

void Scheduler::workerThread()
{
  Singleton& singleton = Singleton::instance();
  chrono::steady_clock::time_point tp_task_config;
  TaskConfig task_config;
  {
    lock_guard<mutex> lock(singleton.mtx_task_config);
    task_config = singleton.task_config;
  }
  tp_task_config = chrono::steady_clock::now();
  for (;;)
  {
    unique_lock<mutex> lock(mtx_workers);
    cv_workers.wait(lock, [this]
      {
        return !tasks_ready_to_run.empty() || !is_worker_running;
      });

    if (!is_worker_running)
      break;

    auto task_data = tasks_ready_to_run.front();
    tasks_ready_to_run.pop();
    lock.unlock();

    //для теста
    {
      lock_guard<mutex> c_locker(mtx_counters);
      ++counters.dequeue_task_count;
    }

    //scope for lock mutex
    {
      lock_guard<mutex> lock2(singleton.mtx_task_config);
      if (tp_task_config < singleton.tp_task_config)
      {
        task_config = singleton.task_config;
        tp_task_config = chrono::steady_clock::now();
        log_level = task_config.conf_params.value(CONF_LOGS_LEVEL).value.toInt();
      }
    }

    if (task_data->id_vstream == 0 || task_data->id_vstream > 0 && task_config.conf_vstream_url.contains(task_data->id_vstream))
      runWorkerTask(task_data, task_config);
  }
}

void Scheduler::runWorkerTask(const shared_ptr<TaskData>& task_data, TaskConfig& task_config)
{
  Singleton& singleton = Singleton::instance();

  if (log_level >= LOGS_VERBOSE)
    singleton.addLog(QString("  --> start runWorkerTask: id_camera=%1").arg(task_data->id_vstream));

  switch (task_data->task_type)
  {
    case TASK_CAPTURE_VSTREAM_FRAME:
    case TASK_GET_FRAME_FROM_URL:
      {
        QString screenshot_url = task_config.conf_vstream_url.value(task_data->id_vstream);
        if (task_data->task_type == TASK_GET_FRAME_FROM_URL)
          screenshot_url = task_data->frame_url;
        if (!screenshot_url.isEmpty())
        {
          if (log_level >= LOGS_VERBOSE)
          {
            if (task_data->task_type == TASK_CAPTURE_VSTREAM_FRAME)
              singleton.addLog(QString("Захват кадра с видео потока %1").arg(task_data->id_vstream));
            if (task_data->task_type == TASK_GET_FRAME_FROM_URL)
              singleton.addLog(QString("Получение кадра по URL: %1").arg(task_data->frame_url));
          }
          auto client = new Wt::Http::Client();
          client->setTimeout(std::chrono::milliseconds{int(1000 * task_config.vstream_conf_params[task_data->id_vstream][CONF_CAPTURE_TIMEOUT].value.toDouble())});
          client->setMaximumResponseSize(8 * 1024 * 1024);  //TODO config
          client->setSslCertificateVerificationEnabled(false);
          client->done().connect(std::bind(&Scheduler::handleHttpScreenshotResponse, this, std::placeholders::_1, std::placeholders::_2,
            client, TaskInfo(*task_data), task_config.vstream_conf_params[task_data->id_vstream][CONF_MAX_CAPTURE_ERROR_COUNT].value.toInt(),
            task_config.vstream_conf_params[task_data->id_vstream][CONF_RETRY_PAUSE].value.toInt()));
          if (!client->get(screenshot_url.toStdString()))
          {
            delete client;
            singleton.addLog(QString("Ошибка! Видео поток %1 недоступен или некорректный URL для захвата кадра: %2").arg(
              task_data->id_vstream).arg(screenshot_url));

            if (task_data->permanent_task)
            {
              singleton.addLog(QString("Пауза на захват кадров на %1 секунд для видео потока %2").arg(
                task_config.vstream_conf_params[task_data->id_vstream][CONF_RETRY_PAUSE].value.toInt()).arg(task_data->id_vstream));
              task_data->error_count = 0;
              addTask(task_data, task_config.vstream_conf_params[task_data->id_vstream][CONF_RETRY_PAUSE].value.toDouble());
            }

            if (task_data->response_continuation != nullptr)
              task_data->response_continuation->haveMoreData();
          }
        }
      }
      break;

    case TASK_RECOGNIZE:
    case TASK_TEST:
      processFrame(task_data.get(), task_config, nullptr);
      break;

    case TASK_REGISTER_DESCRIPTOR:
      {
        if (task_data->response_continuation != nullptr)
        {
          auto rd = Wt::cpp17::any_cast<std::shared_ptr<RegisterDescriptorResponse>>(task_data->response_continuation->data());
          if (rd->face_width == 0)
            rd->face_width = task_data->frame.cols;
          if (rd->face_height == 0)
            rd->face_height = task_data->frame.rows;
          processFrame(task_data.get(), task_config, rd.get());
          task_data->response_continuation->haveMoreData();
          if (rd->id_descriptor > 0)
            singleton.addLog(QString("Создан дескриптор id = %1.").arg(rd->id_descriptor));
        }

      }
      break;

    case TASK_REMOVE_OLD_LOG_FACES:
      {
        if (log_level >= LOGS_EVENTS)
          singleton.addLog("Удаление устаревших сохраненных логов из таблицы log_faces.");

        singleton.removeOldLogFaces();
        addTask(task_data, task_config.conf_params[CONF_CLEAR_OLD_LOG_FACES].value.toFloat() * 3600);
      }
      break;

    default:
      break;
  }

  if (log_level >= LOGS_VERBOSE)
    Singleton::instance().addLog(QString("  --> end runWorkerTask: id_vstream=%1").arg(task_data->id_vstream));
}

void Scheduler::handleHttpScreenshotResponse(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& response,
  Wt::Http::Client* client, TaskInfo task_info, int max_error_count, int retry_pause)
{
  shared_ptr<Wt::Http::Client> client_ptr(client);

  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return;

  if (ec || response.status() != HTTP_SUCCESS)
  {
    if (task_info.task_type == TASK_GET_FRAME_FROM_URL)
      singleton.addLog(QString("Ошибка при получении кадра по URL %3: %1, %2").arg(
        QString::fromStdString(ec.message())).arg(response.status()).arg(task_info.frame_url));
    else
      singleton.addLog(QString("Ошибка при захвате кадра с видео потока id_vstream = %3: %1, %2").arg(
        QString::fromStdString(ec.message())).arg(response.status()).arg(task_info.id_vstream));
    if (task_info.task_type == TASK_CAPTURE_VSTREAM_FRAME)
    {
      if (task_info.error_count < max_error_count)
      {
        ++task_info.error_count;
        singleton.addLog(QString("Повтор захвата кадра с видео потока id_vstream = %1 (попытка %2).").arg(task_info.id_vstream).arg(task_info.error_count));
        auto new_task_data = std::make_shared<TaskData>(task_info);
        addTask(new_task_data, 0.0);
      } else
      {
        if (task_info.permanent_task)
        {
          singleton.addLog(QString("Пауза на захват кадров на %1 секунд для видео потока id_vstream = %2").arg(retry_pause).arg(task_info.id_vstream));
          task_info.error_count = 0;
          auto new_task_data = std::make_shared<TaskData>(task_info);
          addTask(new_task_data, retry_pause);
        } else
        {
          {
            lock_guard<mutex> lock(singleton.mtx_capture);

            //прекращаем захватывать кадры
            singleton.is_capturing.remove(task_info.id_vstream);
          }
        }
      }
    }
    if (task_info.response_continuation != nullptr)
      task_info.response_continuation->haveMoreData();
  } else
  {
    if (log_level >= LOGS_VERBOSE || log_level >= LOGS_EVENTS && task_info.error_count > 0)
      if (task_info.task_type == TASK_GET_FRAME_FROM_URL)
        singleton.addLog(QString("Кадр получен по URL %1, размер: %2").arg(task_info.frame_url).arg(response.body().size()));
      else
        singleton.addLog(QString("Кадр получен от видео потока id_vstream = %1, размер: %2").arg(task_info.id_vstream).arg(response.body().size()));
    string img_data = response.body();
    auto tt0 = std::chrono::steady_clock::now();
    cv::Mat frame = cv::imdecode(std::vector<char>(img_data.begin(), img_data.end()), cv::IMREAD_UNCHANGED);
    auto tt1 = std::chrono::steady_clock::now();
    if (log_level >= LOGS_VERBOSE)
      Singleton::instance().addLog(QString("      decode image time: %1 ms").arg(
        std::chrono::duration_cast<std::chrono::milliseconds>(tt1 - tt0).count()));
    std::shared_ptr<TaskData> new_task_data(nullptr);
    if (task_info.permanent_task)
      new_task_data = std::make_shared<TaskData>(task_info.id_vstream, TASK_RECOGNIZE, true, 0, std::move(frame));
    else
    {
      if (task_info.task_type == TASK_CAPTURE_VSTREAM_FRAME)
        new_task_data = std::make_shared<TaskData>(task_info.id_vstream, TASK_RECOGNIZE,
          task_info.permanent_task, 0, std::move(frame));
      else
        if (task_info.task_type == TASK_GET_FRAME_FROM_URL && task_info.is_registration)
          new_task_data = std::make_shared<TaskData>(task_info.id_vstream, TASK_REGISTER_DESCRIPTOR, false, 0, std::move(frame));
    }
    if (new_task_data != nullptr)
    {
      new_task_data->is_registration = task_info.is_registration;
      new_task_data->response_continuation = task_info.response_continuation;
      new_task_data->frame_data = std::move(img_data);
      addTask(new_task_data, 0.0);
    }
  }
}

void Scheduler::handleHttpDeliveryEventResponse(Wt::AsioWrapper::error_code ec, const Wt::Http::Message& response,
  Wt::Http::Client* client, DeliveryEventType delivery_type, int id_vstream, int id_descriptor)
{
  shared_ptr<Wt::Http::Client> client_ptr(client);

  Singleton& singleton = Singleton::instance();
  if (!singleton.is_working)
    return;

  DeliveryEventResult delivery_result = DeliveryEventResult::SUCCESSFUL;

  if (ec || response.status() != HTTP_SUCCESS && response.status() != HTTP_NO_CONTENT)
    delivery_result = DeliveryEventResult::ERROR;

  singleton.addLogDeliveryEvent(delivery_type, delivery_result, id_vstream, id_descriptor);
}

void Scheduler::processFrame(TaskData* task_data, TaskConfig& task_config, RegisterDescriptorResponse* response)
{
  if (task_data->task_type == TASK_REGISTER_DESCRIPTOR && response == nullptr)
    return;

  //для теста
  //auto tt00 = std::chrono::steady_clock::now();

  Singleton& singleton = Singleton::instance();
  if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
    singleton.addLog(QString("Начало обработки кадра от видео потока: %1").arg(task_data->id_vstream));

  if (!task_data->frame.empty())
  {
    vector<FaceDetection> detected_faces;

    //для теста
    //auto tt0 = std::chrono::steady_clock::now();

    bool is_ok = detectFaces(task_data->id_vstream, task_data->frame, task_config, detected_faces);

    //для теста
    //auto tt1 = std::chrono::steady_clock::now();
    //cout << "__face detection time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;

    if (is_ok)
    {
      vector<FaceData> face_data;
      int recognized_face_count = 0;
      double best_quality = 0.0;
      int best_face_index = -1;
      double best_register_quality = 0.0;
      double best_register_ioa = 0.0;
      int best_register_index = -1;

      //обрабатываем найденные лица
      if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
        singleton.addLog(QString("Обрабатываем найденные лица, количество: %1").arg(detected_faces.size()));

      for (int i = 0; i < detected_faces.size(); ++i)
      {
        if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
          singleton.addLog(QString("      вероятность лица: %1").arg(detected_faces[i].face_confidence));

        int conf_margin = task_config.vstream_conf_params[task_data->id_vstream][CONF_MARGIN].value.toInt();
        cv::Rect work_region = cv::Rect(conf_margin / 100.0 * task_data->frame.cols, conf_margin / 100.0 * task_data->frame.rows,
          task_data->frame.cols - 2 * task_data->frame.cols * conf_margin / 100.0,
          task_data->frame.rows - 2 * task_data->frame.rows * conf_margin / 100.0);
        if (task_config.conf_vstream_region.contains(task_data->id_vstream))
          work_region = work_region & task_config.conf_vstream_region.value(task_data->id_vstream);

        cv::Rect face_rect = cv::Rect(detected_faces[i].bbox[0], detected_faces[i].bbox[1],
          detected_faces[i].bbox[2] - detected_faces[i].bbox[0] + 1, detected_faces[i].bbox[3] - detected_faces[i].bbox[1] + 1);

        face_data.emplace_back(FaceData());
        face_data.back().face_rect = face_rect;

        //проверяем, что лицо полностью находится в рабочей области
        if ((work_region & face_rect) != face_rect)
        {
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog("      лицо не в рабочей области.");

          continue;
        }

        face_data.back().is_work_area = true;

        if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
          singleton.addLog("      лицо в рабочей области.");
        auto landmarks5 = cv::Mat(5, 2, CV_32F, detected_faces[i].landmark);

        face_data.back().landmarks5 = landmarks5.clone();

        //проверяем на фронтальность по маркерам
        if (!isFrontalFace(landmarks5))
        {
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog("      лицо не фронтально по маркерам.");

          continue;
        }

        //проверяем на фронтальность по ориентации головы
        /*auto head_pose = getHeadPose(landmarks5, face_rect);
        bool is_fronal = (fabs(head_pose.angle_r) < task_config.vstream_conf_params[task_data->id_vstream][CONF_ROLL_THRESHOLD].value.toDouble())
            && (fabs(head_pose.angle_p) < task_config.vstream_conf_params[task_data->id_vstream][CONF_PITCH_THRESHOLD].value.toDouble())
            && (fabs(head_pose.angle_y) < task_config.vstream_conf_params[task_data->id_vstream][CONF_YAW_THRESHOLD].value.toDouble());
        if (!is_fronal)
        {
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog(QString("      лицо не фронтально по ориентации: P = %1; R = %2; Y = %3").arg(
              head_pose.angle_p).arg(head_pose.angle_r).arg(head_pose.angle_y));

          continue;
        }

        if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
          singleton.addLog(QString("      лицо фронтально по ориентации: P = %1; R = %2; Y = %3").arg(
            head_pose.angle_p).arg(head_pose.angle_r).arg(head_pose.angle_y));*/

        face_data.back().is_frontal = true;

        //"выравниваем" лицо
        cv::Mat aligned_face = alignFaceAffineTransform(task_data->frame, landmarks5, singleton.face_width, singleton.face_height);

        if (task_data->task_type == TASK_TEST)
          cv::imwrite(singleton.working_path.toStdString() + "/aligned_face.jpg", aligned_face);

        if (aligned_face.cols != singleton.face_width || aligned_face.rows != singleton.face_height)
        {
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog("      не удалось сделать выравнивание лица для получения дескриптора");

          continue;
        }

        //проверяем на размытость
        double laplacian = varianceOfLaplacian(aligned_face);
        face_data.back().laplacian = laplacian;
        if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
          singleton.addLog(QString("      лапласиан: %1").arg(laplacian));
        if (laplacian < task_config.vstream_conf_params[task_data->id_vstream][CONF_LAPLACIAN_THRESHOLD].value.toDouble()
          || laplacian > task_config.vstream_conf_params[task_data->id_vstream][CONF_LAPLACIAN_THRESHOLD_MAX].value.toDouble())
        {
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog("      лицо размыто или слишком четкое.");

          continue;
        }

        face_data.back().is_non_blurry = true;

        if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
          singleton.addLog("      лицо не размыто.");

        //проверяем класс лица (нормальное, в маске, в темных очках)
        //"выравниваем" лицо для инференса класса лица
        int face_class_width = task_config.conf_params[CONF_DNN_FC_INPUT_WIDTH].value.toInt();
        int face_class_height = task_config.conf_params[CONF_DNN_FC_INPUT_HEIGHT].value.toInt();
        cv::Mat aligned_face_class = alignFaceAffineTransform(task_data->frame, landmarks5, face_class_width, face_class_height);
        if (aligned_face_class.cols != face_class_width || aligned_face_class.rows != face_class_height)
        {
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog("      не удалось сделать выравнивание лица для инференса класса");

          continue;
        }

        if (task_data->task_type == TASK_TEST)
          cv::imwrite(singleton.working_path.toStdString() + "/aligned_face_class.jpg", aligned_face_class);

        std::vector<FaceClass> face_classes;
        if (inferFaceClass(task_data->id_vstream, aligned_face_class, task_config, face_classes))
        {
          face_data.back().face_class_index = static_cast<FaceClassIndexes>(face_classes[0].class_index);
          face_data.back().face_class_confidence = face_classes[0].score;
          if (log_level >= LOGS_VERBOSE || task_data->task_type == TASK_TEST)
            singleton.addLog(QString("      класс лица: %1; вероятность: %2").arg(face_classes[0].class_index).arg(face_classes[0].score));
        }

        if (face_data.back().face_class_index == FaceClassIndexes::FACE_NONE
          || face_data.back().face_class_index != FaceClassIndexes::FACE_NORMAL
            && face_data.back().face_class_confidence > task_config.vstream_conf_params[task_data->id_vstream][CONF_FACE_CLASS_CONFIDENCE_THRESHOLD].value.toDouble())
          continue;

        face_data.back().face_class_index = FaceClassIndexes::FACE_NORMAL;

        if (task_data->task_type == TASK_REGISTER_DESCRIPTOR)
        {
          cv::Rect r(response->face_left, response->face_top, response->face_width, response->face_height);
          auto f_intersection = (r & face_data.back().face_rect).area();
          auto f_area = face_data.back().face_rect.area();
          if (f_area > 0)
            face_data.back().ioa = double(f_intersection) / double(f_area);
        }

        //получаем дескриптор лица (биометрический шаблон)
        cv::Mat face_descriptor;

        //для теста
        //auto tt0 = std::chrono::steady_clock::now();

        if (!extractFaceDescriptor(task_data->id_vstream, aligned_face, task_config, face_descriptor))
          continue;

        //для теста
        //auto tt1 = std::chrono::steady_clock::now();
        //cout << "__extract face descriptor time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;

        face_data.back().fd = face_descriptor.clone();
        double norm_l2 = cv::norm(face_descriptor, cv::NORM_L2);
        if (norm_l2 <= 0.0)
          norm_l2 = 1.0;
        face_descriptor = face_descriptor / norm_l2;

        //распознаем лицо
        double max_cos_distance = -2.0;
        int id_descriptor{};

        //для теста
        //auto tt0 = std::chrono::steady_clock::now();

        //scope for lock mutex
        {
          lock_guard<mutex> lock(singleton.mtx_task_config);

          for (auto it_descriptor = singleton.id_vstream_to_id_descriptors[task_data->id_vstream].constBegin(); it_descriptor != singleton.id_vstream_to_id_descriptors[task_data->id_vstream].constEnd(); ++it_descriptor)
          {
            double cos_distance = Singleton::cosineDistance(face_descriptor, singleton.id_descriptor_to_data[*it_descriptor]);
            if (cos_distance > max_cos_distance)
            {
              max_cos_distance = cos_distance;
              id_descriptor = *it_descriptor;
            }
          }
        }

        //для теста
        //auto tt1 = std::chrono::steady_clock::now();
        //cout << "__recognition time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << " ms" << endl;

        face_data.back().cosine_distance = max_cos_distance;

        if (log_level >= LOGS_VERBOSE)
          singleton.addLog(QString("Данные самого похожего: cosine_distance = %1; id_descriptor = %2").arg(max_cos_distance).arg(
            id_descriptor));

        if (task_data->task_type == TASK_TEST)
        {
          singleton.addLog(QString("Данные для теста: cosine_distance = %1; id_descriptor = %2").arg(max_cos_distance).arg(
            id_descriptor));

          continue;
        }

        double conf_tolerance = task_config.vstream_conf_params[task_data->id_vstream][CONF_TOLERANCE].value.toDouble();
        if (id_descriptor == 0 || max_cos_distance < conf_tolerance)
        {
          //лицо не распознано
          if (face_data.back().laplacian > best_quality && recognized_face_count == 0)
          {
            best_quality = face_data.back().laplacian;
            best_face_index = face_data.size() - 1;
          }

        } else
        {
          //лицо распознано
          face_data.back().id_descriptor = id_descriptor;
          ++recognized_face_count;

          if (recognized_face_count == 1 || face_data.back().laplacian > best_quality)
          {
            best_quality = face_data.back().laplacian;
            best_face_index = face_data.size() - 1;
          }
        }

        if (task_data->task_type == TASK_REGISTER_DESCRIPTOR)  //регистрация дескриптора
        {
          if (face_data.back().ioa > 0.999 && face_data.back().laplacian > best_register_quality)
          {
            best_register_quality = face_data.back().laplacian;
            best_register_index = face_data.size() - 1;
          }
          if (fabs(best_register_quality) < 0.001 && face_data.back().ioa > best_register_ioa)
          {
            best_register_ioa = face_data.back().ioa;
            best_register_index = face_data.size() - 1;
          }
        }
      }

      //для теста
      if (best_face_index >= 0 && task_data->task_type == TASK_RECOGNIZE)
        singleton.addLog(QString("    Обнаружены лица детектором: id_vstream = %1").arg(task_data->id_vstream));

      if (best_face_index >= 0 && task_data->task_type == TASK_RECOGNIZE)
      {
        //для теста
        //auto tt0 = std::chrono::steady_clock::now();

        int id_log = singleton.addLogFace(task_data->id_vstream, QDateTime::currentDateTime(), face_data[best_face_index].id_descriptor,
          face_data[best_face_index].laplacian, face_data[best_face_index].face_rect, task_data->frame_data);

        //для теста
        //auto tt1 = std::chrono::steady_clock::now();
        //cout << "__write log time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << " ms" << endl;

        if (id_log > 0 && face_data[best_face_index].id_descriptor > 0)
        {
          //отправляем событие о распознавании лица
          auto client = new Wt::Http::Client();
          client->done().connect(std::bind(&Scheduler::handleHttpDeliveryEventResponse, this, std::placeholders::_1, std::placeholders::_2,
            client, DeliveryEventType::FACE_RECOGNIZED, task_data->id_vstream, face_data[best_face_index].id_descriptor));
          QString url = task_config.conf_vstream_callback_url.value(task_data->id_vstream);
          if (!url.isEmpty())
          {
            Wt::Http::Message body;
            Wt::Json::Object json_data;
            json_data[API::P_FACE_ID] = face_data[best_face_index].id_descriptor;
            json_data[API::P_LOG_FACES_ID] = id_log;
            body.addBodyText(Wt::Json::serialize(json_data, API::INDENT));
            body.setHeader("Content-Type", "application/json");

            if (client->post(url.toStdString(), body))
              singleton.addLog(QString("Отправлено событие о распознавании лица: id_vstream = %1; id_descriptor = %2").arg(
                task_data->id_vstream).arg(face_data[best_face_index].id_descriptor));
            else
            {
              delete client;
              singleton.addLog("Ошибка! Не удалось отправить событие о распознавании лица.");
              singleton.addLogDeliveryEvent(DeliveryEventType::FACE_RECOGNIZED, DeliveryEventResult::ERROR, task_data->id_vstream,
                face_data[best_face_index].id_descriptor);
            }
          }
        }
      }

      if (task_data->task_type == TASK_REGISTER_DESCRIPTOR)
      {
        if (best_register_index < 0)
        {
          response->comments = task_config.conf_params[CONF_COMMENTS_NO_FACES].value.toString();
        } else
        {
          if (!face_data[best_register_index].is_work_area)
            response->comments = task_config.conf_params[CONF_COMMENTS_PARTIAL_FACE].value.toString();
          else
            if (!face_data[best_register_index].is_frontal)
              response->comments = task_config.conf_params[CONF_COMMENTS_NON_FRONTAL_FACE].value.toString();
            else
              if (!face_data[best_register_index].is_non_blurry)
                response->comments = task_config.conf_params[CONF_COMMENTS_BLURRY_FACE].value.toString();
              else
                if (face_data[best_register_index].face_class_index != FaceClassIndexes::FACE_NORMAL)
                  response->comments = task_config.conf_params[CONF_COMMENTS_NON_NORMAL_FACE_CLASS].value.toString();
                else
                {
                  //все ок, регистрируем дескриптор
                  cv::Rect r = enlargeFaceRect(face_data[best_register_index].face_rect, task_config.vstream_conf_params[task_data->id_vstream][CONF_FACE_ENLARGE_SCALE].value.toDouble());
                  r = r & cv::Rect(0, 0, task_data->frame.cols, task_data->frame.rows);
                  if (face_data[best_register_index].cosine_distance > 0.999)
                    response->id_descriptor = face_data[best_register_index].id_descriptor;
                  else
                    response->id_descriptor = singleton.addFaceDescriptor(task_data->id_vstream, face_data[best_register_index].fd, task_data->frame(r));

                  if (response->id_descriptor > 0)
                  {
                    if (face_data[best_register_index].id_descriptor != response->id_descriptor)
                      response->comments = task_config.conf_params[CONF_COMMENTS_NEW_DESCRIPTOR].value.toString();
                    else
                      response->comments = task_config.conf_params[CONF_COMMENTS_DESCRIPTOR_EXISTS].value.toString();
                    response->face_image = task_data->frame(r).clone();
                    response->face_left = face_data[best_register_index].face_rect.x;
                    response->face_top = face_data[best_register_index].face_rect.y;
                    response->face_width = face_data[best_register_index].face_rect.width;
                    response->face_height = face_data[best_register_index].face_rect.height;
                  } else
                    response->comments = task_config.conf_params[CONF_COMMENTS_DESCRIPTOR_CREATION_ERROR].value.toString();
                }
        }
      }

      //отрисовка рамки и маркеров, сохранение кадра
      if (task_data->task_type == TASK_TEST)
      {
        for (auto&& f : face_data)
        {
          if (!f.landmarks5.empty())
            for (int k = 0; k < 5; ++k)
              cv::circle(task_data->frame, cv::Point(f.landmarks5.at<float>(k, 0), f.landmarks5.at<float>(k, 1)), 1, cv::Scalar(255 * (k * 2 > 2), 255 * (k * 2 > 0 && k * 2 < 8), 255 * (k * 2 < 6)), 4);
          cv::rectangle(task_data->frame, f.face_rect, cv::Scalar(0, 200, 0));
        }

        auto frame_indx = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
        cv::imwrite(singleton.working_path.toStdString() + "/frame_" + std::to_string(frame_indx) + ".jpg", task_data->frame);
        singleton.addLog(QString("Frame index: %1").arg(frame_indx));
      }
    } else
    {
      if (task_data->task_type == TASK_REGISTER_DESCRIPTOR)
      {
        response->id_descriptor = 0;
        response->comments = task_config.conf_params[CONF_COMMENTS_INFERENCE_ERROR].value.toString();
      }
    }
  }

  //продолжаем, если требуется, цепочку заданий
  if (task_data->task_type == TASK_RECOGNIZE)
  {
    bool do_capture = false;

    //scope for lock mutex
    {
      lock_guard<mutex> lock(singleton.mtx_capture);
      auto tp_start_motion = singleton.id_vstream_to_start_motion.value(task_data->id_vstream);
      auto tp_end_motion = singleton.id_vstream_to_end_motion.value(task_data->id_vstream);
      if (tp_start_motion <= tp_end_motion
        && tp_end_motion + std::chrono::milliseconds(int(1000 * task_config.vstream_conf_params[task_data->id_vstream][CONF_PROCESS_FRAMES_INTERVAL].value.toDouble())) < std::chrono::steady_clock::now())
      {
        //прекращаем захватывать кадры
        singleton.is_capturing.remove(task_data->id_vstream);
      } else
        do_capture = true;
    }

    if (do_capture)
    {
      auto new_task_data = std::make_shared<TaskData>(task_data->id_vstream, TASK_CAPTURE_VSTREAM_FRAME);
      addTask(new_task_data, task_config.vstream_conf_params[task_data->id_vstream][CONF_DELAY_BETWEEN_FRAMES].value.toDouble());
    } else
      singleton.addLog(QString("Конец захвата кадров с видео потока %1").arg(task_data->id_vstream));
  }

  if (log_level >= LOGS_VERBOSE  || task_data->task_type == TASK_TEST)
    singleton.addLog(QString("Конец обработки кадра от видео потока: %1").arg(task_data->id_vstream));

  //для теста
  //auto tt01 = std::chrono::steady_clock::now();
  //cout << "__processFrame time: " << std::chrono::duration<double, std::milli>(tt01 - tt00).count() << endl;
}
