#include "tasks.h"
#include "cpr/cpr.h"
#include "http_client.h"

namespace tc = triton::client;

using namespace std;

namespace
{
  constexpr const int HTTP_SUCCESS = 200;
  constexpr const int HTTP_NO_CONTENT = 204;

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
  bool detectFaces(int id_vstream, const cv::Mat& frame, vector<FaceDetection>& detected_faces)
  {
    Singleton& singleton = Singleton::instance();
    String server_url;
    String model_name;
    String input_tensor_name;
    int input_width{};
    int input_height{};
    double face_confidence_threshold{};

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);
      server_url = singleton.getConfigParamValue<String>(CONF_DNN_FD_INFERENCE_SERVER, id_vstream);
      model_name = singleton.getConfigParamValue<String>(CONF_DNN_FD_MODEL_NAME);
      input_tensor_name = singleton.getConfigParamValue<String>(CONF_DNN_FD_INPUT_TENSOR_NAME);
      input_width =  singleton.getConfigParamValue<int>(CONF_DNN_FD_INPUT_WIDTH);
      input_height = singleton.getConfigParamValue<int>(CONF_DNN_FD_INPUT_HEIGHT);
      face_confidence_threshold = singleton.getConfigParamValue<double>(CONF_FACE_CONFIDENCE_THRESHOLD, id_vstream);
    }

    unique_ptr<tc::InferenceServerHttpClient> triton_client;
    auto err = tc::InferenceServerHttpClient::Create(&triton_client, server_url, false);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать клиента для инференса: $0", err.Message()));
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
          input_buffer[c * input_height * input_width + h * input_width + w] = (static_cast<float>(pr_img.at<cv::Vec3b>(h, w)[2 - c]) - 127.5f) / 128.0;

    vector<uint8_t> input_data(input_size * sizeof(float));
    memcpy(input_data.data(), input_buffer.data(), input_data.size());
    std::vector<int64_t> shape = {1, channels, input_height, input_width};
    tc::InferInput* input;
    err = tc::InferInput::Create(&input, input_tensor_name, shape, "FP32");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать входные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferInput> input_ptr(input);

    tc::InferRequestedOutput* output497;
    err = tc::InferRequestedOutput::Create(&output497, "497");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output497_ptr(output497);

    tc::InferRequestedOutput* output494;
    err = tc::InferRequestedOutput::Create(&output494, "494");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output494_ptr(output494);

    tc::InferRequestedOutput* output477;
    err = tc::InferRequestedOutput::Create(&output477, "477");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output477_ptr(output477);

    tc::InferRequestedOutput* output454;
    err = tc::InferRequestedOutput::Create(&output454, "454");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output454_ptr(output454);

    tc::InferRequestedOutput* output451;
    err = tc::InferRequestedOutput::Create(&output451, "451");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output451_ptr(output451);

    tc::InferRequestedOutput* output474;
    err = tc::InferRequestedOutput::Create(&output474, "474");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output474_ptr(output474);

    tc::InferRequestedOutput* output448;
    err = tc::InferRequestedOutput::Create(&output448, "448");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output448_ptr(output448);

    tc::InferRequestedOutput* output500;
    err = tc::InferRequestedOutput::Create(&output500, "500");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output500_ptr(output500);

    tc::InferRequestedOutput* output471;
    err = tc::InferRequestedOutput::Create(&output471, "471");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output471_ptr(output471);

    std::vector<tc::InferInput*> inputs = {input_ptr.get()};
    std::vector<const tc::InferRequestedOutput*> outputs = {
      output497_ptr.get(), output494_ptr.get(), output477_ptr.get(), output454_ptr.get(), output451_ptr.get()
      , output474_ptr.get(), output448_ptr.get(), output500_ptr.get(), output471_ptr.get()
    };
    err = input_ptr->AppendRaw(input_data);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось установить входные данные: $0", err.Message()));
      return false;
    }

    tc::InferOptions options(model_name);
    options.model_version_ = "";
    tc::InferResult* result;

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
      singleton.addLog(absl::Substitute("Ошибка! Не удалось отправить синхронный запрос инференса: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferResult> result_ptr(result);

    if (!result_ptr->RequestStatus().IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось получить результат инференса: $0", result_ptr->RequestStatus().Message()));
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

  bool inferFaceClass(int id_vstream, const cv::Mat& aligned_face, std::vector<FaceClass>& face_classes)
  {
    Singleton& singleton = Singleton::instance();

    String server_url;
    String model_name;
    String input_tensor_name;
    int input_width{};
    int input_height{};
    String output_tensor_name;
    int class_count{};

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);
      server_url = singleton.getConfigParamValue<String>(CONF_DNN_FC_INFERENCE_SERVER, id_vstream);
      model_name = singleton.getConfigParamValue<String>(CONF_DNN_FC_MODEL_NAME);
      input_tensor_name = singleton.getConfigParamValue<String>(CONF_DNN_FC_INPUT_TENSOR_NAME);
      input_width = singleton.getConfigParamValue<int>(CONF_DNN_FC_INPUT_WIDTH);
      input_height = singleton.getConfigParamValue<int>(CONF_DNN_FC_INPUT_HEIGHT);
      output_tensor_name = singleton.getConfigParamValue<String>(CONF_DNN_FC_OUTPUT_TENSOR_NAME);
      class_count = singleton.getConfigParamValue<int>(CONF_DNN_FC_OUTPUT_SIZE);
    }

    unique_ptr<tc::InferenceServerHttpClient> triton_client;
    auto err = tc::InferenceServerHttpClient::Create(&triton_client, server_url, false);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать клиента для инференса: $0", err.Message()));
      return false;
    }

    int channels = 3;
    int input_size = channels * input_width * input_height;
    vector<float> input_buffer(input_size);

    for (int c = 0; c < channels; ++c)
      for (int h = 0; h < input_height; ++h)
        for (int w = 0; w < input_width; ++w)
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
          input_buffer[c * input_height * input_width + h * input_width + w] =
            (static_cast<float>(aligned_face.at<cv::Vec3b>(h, w)[2 - c]) / 255.0f - mean) / std_d;
        }

    vector<uint8_t> input_data(input_size * sizeof(float));
    memcpy(input_data.data(), input_buffer.data(), input_data.size());
    std::vector<int64_t> shape = {1, channels, input_height, input_width};
    tc::InferInput* input;
    err = tc::InferInput::Create(&input, input_tensor_name, shape, "FP32");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать входные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferInput> input_ptr(input);

    tc::InferRequestedOutput* output;
    err = tc::InferRequestedOutput::Create(&output, output_tensor_name);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output_ptr(output);
    std::vector<tc::InferInput*> inputs = {input_ptr.get()};
    std::vector<const tc::InferRequestedOutput*> outputs = {output_ptr.get()};
    err = input_ptr->AppendRaw(input_data);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось установить входные данные: $0", err.Message()));
      return false;
    }

    tc::InferOptions options(model_name);
    options.model_version_ = "";
    tc::InferResult* result;

    //для теста
    //auto tt0 = std::chrono::steady_clock::now();

    err = triton_client->Infer(&result, options, inputs, outputs);

    //для теста
    //auto tt1 = std::chrono::steady_clock::now();
    //cout << "  inference time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << endl;

    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось отправить синхронный запрос инференса: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferResult> result_ptr(result);

    if (!result_ptr->RequestStatus().IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось получить результат инференса: $0", result_ptr->RequestStatus().Message()));
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

  bool extractFaceDescriptor(int id_vstream, const cv::Mat& aligned_face, FaceDescriptor& face_descriptor)
  {
    Singleton& singleton = Singleton::instance();

    String server_url;
    String model_name;
    String input_tensor_name;
    int input_width{};
    int input_height{};
    String output_tensor_name;

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);
      server_url = singleton.getConfigParamValue<String>(CONF_DNN_FR_INFERENCE_SERVER, id_vstream);
      model_name = singleton.getConfigParamValue<String>(CONF_DNN_FR_MODEL_NAME);
      input_tensor_name = singleton.getConfigParamValue<String>(CONF_DNN_FR_INPUT_TENSOR_NAME);
      input_width = singleton.getConfigParamValue<int>(CONF_DNN_FR_INPUT_WIDTH);
      input_height = singleton.getConfigParamValue<int>(CONF_DNN_FR_INPUT_HEIGHT);
      output_tensor_name = singleton.getConfigParamValue<String>(CONF_DNN_FR_OUTPUT_TENSOR_NAME);
    }

    unique_ptr<tc::InferenceServerHttpClient> triton_client;
    auto err = tc::InferenceServerHttpClient::Create(&triton_client, server_url, false);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать клиента для инференса: $0", err.Message()));
      return false;
    }

    int channels = 3;
    int input_size = channels * input_width * input_height;
    vector<float> input_buffer(input_size);
    for (int c = 0; c < channels; ++c)
      for (int h = 0; h < input_height; ++h)
        for (int w = 0; w < input_width; ++w)
          if (model_name == "arcface")
            input_buffer[c * input_height * input_width + h * input_width + w] = static_cast<float>(aligned_face.at<cv::Vec3b>(h, w)[2 - c]) / 127.5f - 1.0f;
          else
            input_buffer[c * input_height * input_width + h * input_width + w] = (static_cast<float>(aligned_face.at<cv::Vec3b>(h, w)[2 - c]) - 127.5f) / 128.0f;
    vector<uint8_t> input_data(input_size * sizeof(float));
    memcpy(input_data.data(), input_buffer.data(), input_data.size());
    std::vector<int64_t> shape = {1, channels, input_height, input_width};
    tc::InferInput* input;
    err = tc::InferInput::Create(&input, input_tensor_name, shape, "FP32");
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать входные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferInput> input_ptr(input);

    tc::InferRequestedOutput* output;
    err = tc::InferRequestedOutput::Create(&output, output_tensor_name);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось создать выходные данные: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferRequestedOutput> output_ptr(output);
    std::vector<tc::InferInput*> inputs = {input_ptr.get()};
    std::vector<const tc::InferRequestedOutput*> outputs = {output_ptr.get()};
    err = input_ptr->AppendRaw(input_data);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось установить входные данные: $0", err.Message()));
      return false;
    }

    tc::InferOptions options(model_name);
    options.model_version_ = "";
    tc::InferResult* result;
    err = triton_client->Infer(&result, options, inputs, outputs);
    if (!err.IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось отправить синхронный запрос инференса: $0", err.Message()));
      return false;
    }
    std::shared_ptr<tc::InferResult> result_ptr(result);

    if (!result_ptr->RequestStatus().IsOk())
    {
      singleton.addLog(absl::Substitute("Ошибка! Не удалось получить результат инференса: $0", result_ptr->RequestStatus().Message()));
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

  struct SGroupFaceData
  {
    double cosine_distance = -2.0;
    int id_descriptor = 0;
  };

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
    HashMap<int, SGroupFaceData> sg_descriptors;
  };

  //функция для удаления ненужных скриншотов и других данных
  void removeFiles(const String& path, DateTime dt)
  {
    HashSet<String> img_extensions = {".png", ".jpg", ".jpeg", ".bmp", ".ppm", ".tiff", ".dat", ".json"};
    for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(path))
      if (dir_entry.is_regular_file() && img_extensions.contains(dir_entry.path().extension().string()))
      {
        auto t = std::chrono::file_clock::to_sys(dir_entry.last_write_time());
        if (t < absl::ToChronoTime(dt))
          std::filesystem::remove(dir_entry);
      }
  }

  //функция для записи данных события
  void saveEventData(absl::string_view event_id, const vector<FaceData>& faces, int best_index)
  {
    auto& singleton = Singleton::instance();
    if (!faces.empty())
    {
      auto path_part = absl::Substitute("$0/$1/$2/$3", event_id[0], event_id[1], event_id[2], event_id);
      ofstream ff(singleton.screenshot_path / (path_part + ".dat"), std::ios::binary);
      crow::json::wvalue::list json_faces;
      for (int32_t i = 0; i < faces.size(); ++i)
      {
        crow::json::wvalue::list landmarks5;
        if (!faces[i].landmarks5.empty())
          for (int k = 0; k < 5; ++k)
          {
            landmarks5.push_back(faces[i].landmarks5.at<float>(k, 0));
            landmarks5.push_back(faces[i].landmarks5.at<float>(k, 1));
          }
        json_faces.push_back(
          {
            {"left", faces[i].face_rect.tl().x},
            {"top", faces[i].face_rect.tl().y},
            {"width", faces[i].face_rect.width},
            {"height", faces[i].face_rect.height},
            {"laplacian", faces[i].laplacian},
            {"landmarks5", landmarks5},
            {"face_class_index", static_cast<int>(faces[i].face_class_index)},
            {"id_descriptor", faces[i].id_descriptor},
            {"face_class_confidence", faces[i].face_class_confidence},
            {"is_frontal", faces[i].is_frontal},
            {"is_non_blurry", faces[i].is_non_blurry},
            {"is_work_area", faces[i].is_work_area}
          });

        if (!faces[i].fd.empty())
        {
          //пишем данные дескриптора в файл
          ff.write(event_id.data(), static_cast<streamsize>(event_id.size()));
          ff.write(reinterpret_cast<char*>(&i), sizeof(int32_t));
          ff.write((const char*)(faces[i].fd.data), static_cast<streamsize>(singleton.descriptor_size * sizeof(float)));
        }
      }

      //пишем JSON-данные события
      crow::json::wvalue json_data{
        {"best_face_index", best_index},
        {"faces", json_faces}
      };
      ofstream f_json(singleton.screenshot_path / (path_part + ".json"));
      f_json << json_data.dump();
    }
  }
}

concurrencpp::result<void> checkMotion(TaskData task_data)
{
  auto& singleton = Singleton::instance();
  try
  {
    if (task_data.delay_s > 0)
    {
      co_await singleton.runtime->timer_queue()->make_delay_object(
        std::chrono::milliseconds(static_cast<int>(1000 * task_data.delay_s)),
        Singleton::instance().runtime->thread_pool_executor());
    }

    if (!singleton.is_working.load(std::memory_order_relaxed))
      co_return;

    bool do_task = false;

    //scope for lock mutex
    {
      scoped_lock lock(singleton.mtx_capture);
      singleton.is_door_open.erase(task_data.id_vstream);
      auto tp_start_motion = singleton.id_vstream_to_start_motion.contains(task_data.id_vstream)
        ? singleton.id_vstream_to_start_motion.at(task_data.id_vstream) : time_point{};
      auto tp_end_motion = singleton.id_vstream_to_end_motion.contains(task_data.id_vstream)
        ? singleton.id_vstream_to_end_motion.at(task_data.id_vstream) : time_point{};
      if (tp_start_motion > tp_end_motion)
      {
        do_task = true;
        singleton.is_capturing.insert(task_data.id_vstream);
      } else
        singleton.is_capturing.erase(task_data.id_vstream);
    }

    if (do_task)
    {
      task_data.task_type = TASK_RECOGNIZE;
      task_data.delay_s = 0.0;
      singleton.runtime->thread_pool_executor()->submit(processFrame, task_data, nullptr, nullptr);
      singleton.addLog(absl::Substitute(API::LOG_START_MOTION, task_data.id_vstream));
    }

  } catch (const concurrencpp::errors::broken_task& e)
  {
    //ничего не делаем
  } catch (const concurrencpp::errors::runtime_shutdown& e)
  {
    //ничего не делаем
  } catch (const std::exception& e)
  {
    singleton.addLog(e.what());
    scoped_lock lock(singleton.mtx_capture);

    //убираем открытие двери
    singleton.is_door_open.erase(task_data.id_vstream);
  }
}

concurrencpp::result<void> processFrame(TaskData task_data, std::shared_ptr<RegisterDescriptorResponse> response,
  std::shared_ptr<ProcessFrameResult> process_result)
{
  auto& singleton = Singleton::instance();
  try
  {
    if (task_data.task_type == TASK_REGISTER_DESCRIPTOR && response == nullptr)
      co_return;

    if (task_data.task_type == TASK_PROCESS_FRAME && process_result == nullptr)
      co_return;

    if (task_data.delay_s > 0)
    {
      co_await singleton.runtime->timer_queue()->make_delay_object(
        std::chrono::milliseconds(static_cast<int>(1000 * task_data.delay_s)),
        Singleton::instance().runtime->thread_pool_executor());
    }

    if (task_data.task_type == TASK_REGISTER_DESCRIPTOR || task_data.task_type == TASK_PROCESS_FRAME)
    {
      if (!singleton.is_working.load(std::memory_order_relaxed))
        co_return;

      co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());
    }

    int log_level{};
    String url;
    int32_t request_timeout_ms{};
    int max_capture_error_count{};
    double conf_margin{};
    bool conf_has_vstream_region{};
    cv::Rect conf_work_region{};
    double conf_laplacian_threshold{};
    double conf_laplacian_threshold_max{};
    int face_class_width{};
    int face_class_height{};
    double conf_face_class_confidence_threshold{};
    double conf_tolerance{};
    String conf_callback_url;
    int32_t conf_callback_timeout_ms{};
    String conf_comments_no_faces;
    String conf_comments_partial_face;
    String conf_comments_non_frontal_face;
    String conf_comments_blurry_face;
    String conf_comments_non_normal_face_class;
    String conf_comments_new_descriptor;
    String conf_comments_descriptor_exists;
    String conf_comments_descriptor_creation_error;
    String conf_comments_inference_error;
    double conf_face_enlarge_scale{};
    double conf_process_frames_interval{};
    double conf_delay_between_frames{};
    HashMap<int, String> conf_sgroup_callback_url;

    //scope for lock mutex
    {
      ReadLocker lock(singleton.mtx_task_config);
      log_level = static_cast<LogsLevel>(singleton.getConfigParamValue<int>(CONF_LOGS_LEVEL, task_data.id_vstream));

      if (task_data.task_type == TASK_RECOGNIZE)
      {
        auto it = singleton.task_config.conf_vstream_url.find(task_data.id_vstream);
        if (it != singleton.task_config.conf_vstream_url.end())
          url = it->second;
      } else
        url = task_data.frame_url;

      request_timeout_ms = static_cast<int32_t>(1000 *
        singleton.getConfigParamValue<double>(CONF_CAPTURE_TIMEOUT, task_data.id_vstream));
      max_capture_error_count = singleton.getConfigParamValue<int>(CONF_MAX_CAPTURE_ERROR_COUNT, task_data.id_vstream);
      conf_margin = singleton.getConfigParamValue<double>(CONF_MARGIN, task_data.id_vstream);
      conf_has_vstream_region = singleton.task_config.conf_vstream_region.contains(task_data.id_vstream);
      if (conf_has_vstream_region)
        conf_work_region = singleton.task_config.conf_vstream_region.at(task_data.id_vstream);
      conf_laplacian_threshold = singleton.getConfigParamValue<double>(CONF_LAPLACIAN_THRESHOLD, task_data.id_vstream);
      conf_laplacian_threshold_max = singleton.getConfigParamValue<double>(CONF_LAPLACIAN_THRESHOLD_MAX,
        task_data.id_vstream);
      face_class_width = singleton.getConfigParamValue<int>(CONF_DNN_FC_INPUT_WIDTH);
      face_class_height = singleton.getConfigParamValue<int>(CONF_DNN_FC_INPUT_HEIGHT);
      conf_face_class_confidence_threshold = singleton.getConfigParamValue<double>(CONF_FACE_CLASS_CONFIDENCE_THRESHOLD,
        task_data.id_vstream);
      conf_tolerance = singleton.getConfigParamValue<double>(CONF_TOLERANCE, task_data.id_vstream);

      {
        auto it = singleton.task_config.conf_vstream_callback_url.find(task_data.id_vstream);
        if (it != singleton.task_config.conf_vstream_callback_url.end())
          conf_callback_url = it->second;
      }

      conf_callback_timeout_ms = static_cast<int32_t>(1000 *
        singleton.getConfigParamValue<double>(CONF_CALLBACK_TIMEOUT, task_data.id_vstream));
      conf_comments_no_faces = singleton.getConfigParamValue<String>(CONF_COMMENTS_NO_FACES);
      conf_comments_partial_face = singleton.getConfigParamValue<String>(CONF_COMMENTS_PARTIAL_FACE);
      conf_comments_non_frontal_face = singleton.getConfigParamValue<String>(CONF_COMMENTS_NON_FRONTAL_FACE);
      conf_comments_blurry_face = singleton.getConfigParamValue<String>(CONF_COMMENTS_BLURRY_FACE);
      conf_comments_non_normal_face_class = singleton.getConfigParamValue<String>(CONF_COMMENTS_NON_NORMAL_FACE_CLASS);
      conf_comments_new_descriptor = singleton.getConfigParamValue<String>(CONF_COMMENTS_NEW_DESCRIPTOR);
      conf_comments_descriptor_exists = singleton.getConfigParamValue<String>(CONF_COMMENTS_DESCRIPTOR_EXISTS);
      conf_comments_descriptor_creation_error = singleton.getConfigParamValue<String>(
        CONF_COMMENTS_DESCRIPTOR_CREATION_ERROR);
      conf_comments_inference_error = singleton.getConfigParamValue<String>(CONF_COMMENTS_INFERENCE_ERROR);
      conf_face_enlarge_scale = singleton.getConfigParamValue<double>(CONF_FACE_ENLARGE_SCALE, task_data.id_vstream);
      conf_process_frames_interval = singleton.getConfigParamValue<double>(CONF_PROCESS_FRAMES_INTERVAL,
        task_data.id_vstream);
      conf_delay_between_frames = singleton.getConfigParamValue<double>(CONF_DELAY_BETWEEN_FRAMES,
        task_data.id_vstream);
      conf_sgroup_callback_url = singleton.task_config.conf_sgroup_callback_url;
    }

    //получение кадра по URL
    String image_data;
    if (task_data.is_base64)
    {
      if (!absl::Base64Unescape(task_data.frame_url, &image_data))
      {
        if (task_data.task_type == TASK_RECOGNIZE)
        {
          scoped_lock lock(singleton.mtx_capture);

          //прекращаем захватывать кадры
          singleton.is_capturing.erase(task_data.id_vstream);
        }

        co_return;
      }
    } else
    {
      //переключаемся в фоновый режим
      if (!singleton.is_working.load(std::memory_order_relaxed))
        co_return;
      co_await concurrencpp::resume_on(singleton.runtime->background_executor());

      cpr::SslOptions ssl_opts = cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false},
        cpr::ssl::VerifyStatus{false});
      cpr::Response response_image = cpr::Get(cpr::Url{url}, cpr::Timeout(request_timeout_ms), ssl_opts);
      if (response_image.status_code != HTTP_SUCCESS)
      {
        if (task_data.task_type == TASK_RECOGNIZE)
        {
          singleton.addLog(absl::Substitute("Ошибка при захвате кадра с видео потока id_vstream = $0: $1",
            task_data.id_vstream, response_image.status_line));
          if (task_data.error_count < max_capture_error_count)
          {
            ++task_data.error_count;
            singleton.addLog(absl::Substitute("Повтор захвата кадра с видео потока id_vstream = $0 (попытка $1).",
              task_data.id_vstream, task_data.error_count));
            task_data.delay_s = 0.0;
            singleton.runtime->thread_pool_executor()->submit(processFrame, task_data, nullptr, nullptr);
          } else
          {
            scoped_lock lock(singleton.mtx_capture);

            //прекращаем захватывать кадры
            singleton.is_capturing.erase(task_data.id_vstream);
          }
        } else
          singleton.addLog(absl::Substitute("Ошибка при получении кадра по URL $0: $1", url, response_image.status_line));

        co_return;
      }

      image_data = std::move(response_image.text);

      //переключаемся в основной режим
      if (!singleton.is_working.load(std::memory_order_relaxed))
        co_return;
      co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());
    }

    if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
    {
      if (task_data.id_sgroup == 0)
        singleton.addLog(absl::Substitute("Начало обработки кадра от видео потока: $0", task_data.id_vstream));
      else
        singleton.addLog(absl::Substitute("Начало обработки кадра для специальной группы: $0", task_data.id_sgroup));
    }

    //для теста
    //auto tt00 = std::chrono::steady_clock::now();

    //декодирование изображения
    cv::Mat frame = cv::imdecode(std::vector<char>(image_data.begin(), image_data.end()), cv::IMREAD_COLOR);
    if (!image_data.empty())
    {
      if (task_data.task_type == TASK_REGISTER_DESCRIPTOR)
      {
        //если не указана область поиска лица, то ищем по всему изображению
        if (response->face_width == 0)
          response->face_width = frame.cols;
        if (response->face_height == 0)
          response->face_height = frame.rows;
      }

      vector<FaceDetection> detected_faces;
      DNNStatsData dnn_stats_data;

      //для теста
      //auto tt0 = std::chrono::steady_clock::now();

      //переключаемся в фоновый режим
      if (!singleton.is_working.load(std::memory_order_relaxed))
        co_return;
      co_await concurrencpp::resume_on(singleton.runtime->background_executor());

      bool is_ok = detectFaces(task_data.id_vstream, frame, detected_faces);

      //переключаемся в основной режим
      if (!singleton.is_working.load(std::memory_order_relaxed))
        co_return;
      co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());

      //для сбора статистики инференса
      ++dnn_stats_data.fd_count;

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
        bool has_sgroup_events = false;

        //обрабатываем найденные лица
        if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
          singleton.addLog(absl::Substitute("Обрабатываем найденные лица, количество: $0", detected_faces.size()));

        for (int i = 0; i < detected_faces.size(); ++i)
        {
          if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
            singleton.addLog(absl::Substitute("      вероятность лица: $0", detected_faces[i].face_confidence));

          cv::Rect work_region = cv::Rect(conf_margin / 100.0 * frame.cols, conf_margin / 100.0 * frame.rows,
            frame.cols - 2 * frame.cols * conf_margin / 100.0,
            frame.rows - 2 * frame.rows * conf_margin / 100.0);
          if (conf_has_vstream_region)
            work_region = work_region & conf_work_region;

          cv::Rect face_rect = cv::Rect(detected_faces[i].bbox[0], detected_faces[i].bbox[1],
            detected_faces[i].bbox[2] - detected_faces[i].bbox[0] + 1,
            detected_faces[i].bbox[3] - detected_faces[i].bbox[1] + 1);

          face_data.emplace_back(FaceData());
          face_data.back().face_rect = face_rect;

          //проверяем, что лицо полностью находится в рабочей области
          if ((work_region & face_rect) != face_rect)
          {
            if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
              singleton.addLog("      лицо не в рабочей области.");

            continue;
          }

          face_data.back().is_work_area = true;

          if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
            singleton.addLog("      лицо в рабочей области.");
          auto landmarks5 = cv::Mat(5, 2, CV_32F, detected_faces[i].landmark);

          face_data.back().landmarks5 = landmarks5.clone();

          //проверяем на фронтальность по маркерам
          if (!isFrontalFace(landmarks5))
          {
            if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
              singleton.addLog("      лицо не фронтально по маркерам.");

            continue;
          }

          //"выравниваем" лицо
          cv::Mat aligned_face = alignFaceAffineTransform(frame, landmarks5, singleton.face_width,
            singleton.face_height);

          if (aligned_face.cols != singleton.face_width || aligned_face.rows != singleton.face_height)
          {
            if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
              singleton.addLog("      не удалось сделать выравнивание лица для получения дескриптора");

            continue;
          }

          if (task_data.task_type == TASK_TEST)
            cv::imwrite(singleton.working_path + "/aligned_face.jpg", aligned_face);

          face_data.back().is_frontal = true;

          //проверяем на размытость
          double laplacian = varianceOfLaplacian(aligned_face);
          face_data.back().laplacian = laplacian;
          if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
            singleton.addLog(absl::Substitute("      лапласиан: $0", laplacian));
          if (laplacian < conf_laplacian_threshold
            || laplacian > conf_laplacian_threshold_max)
          {
            if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
              singleton.addLog("      лицо размыто или слишком четкое.");

            continue;
          }

          face_data.back().is_non_blurry = true;

          if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
            singleton.addLog("      лицо не размыто.");

          //проверяем класс лица (нормальное, в маске, в темных очках)
          //"выравниваем" лицо для инференса класса лица
          cv::Mat aligned_face_class = alignFaceAffineTransform(frame, landmarks5, face_class_width, face_class_height);
          if (aligned_face_class.cols != face_class_width || aligned_face_class.rows != face_class_height)
          {
            if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
              singleton.addLog("      не удалось сделать выравнивание лица для инференса класса");

            continue;
          }

          if (task_data.task_type == TASK_TEST)
            cv::imwrite(singleton.working_path + "/aligned_face_class.jpg", aligned_face_class);

          std::vector<FaceClass> face_classes;

          //переключаемся в фоновый режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->background_executor());

          bool infer_face_class_result = inferFaceClass(task_data.id_vstream, aligned_face_class, face_classes);

          //переключаемся в основной режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());

          //для сбора статистики инференса
          ++dnn_stats_data.fc_count;

          if (infer_face_class_result)
          {
            face_data.back().face_class_index = static_cast<FaceClassIndexes>(face_classes[0].class_index);
            face_data.back().face_class_confidence = face_classes[0].score;
            if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
              singleton.addLog(absl::Substitute("      класс лица: $0; вероятность: $1", face_classes[0].class_index,
                face_classes[0].score));
          }

          if (face_data.back().face_class_index == FaceClassIndexes::FACE_NONE
            || face_data.back().face_class_index != FaceClassIndexes::FACE_NORMAL
            && face_data.back().face_class_confidence > conf_face_class_confidence_threshold)
            continue;

          face_data.back().face_class_index = FaceClassIndexes::FACE_NORMAL;

          if (task_data.task_type == TASK_REGISTER_DESCRIPTOR)
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
          //tt0 = std::chrono::steady_clock::now();

          //переключаемся в фоновый режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->background_executor());

          bool infer_face_descriptor_result = extractFaceDescriptor(task_data.id_vstream, aligned_face,
            face_descriptor);

          //переключаемся в основной режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());

          if (!infer_face_descriptor_result)
            continue;

          //для сбора статистики инференса
          ++dnn_stats_data.fr_count;

          //для теста
          //tt1 = std::chrono::steady_clock::now();
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
          //tt0 = std::chrono::steady_clock::now();

          //scope for lock mutex
          {
            ReadLocker lock(singleton.mtx_task_config);

            if (task_data.id_vstream > 0)
              for (auto it_descriptor = singleton.id_vstream_to_id_descriptors[task_data.id_vstream].cbegin();
                it_descriptor != singleton.id_vstream_to_id_descriptors[task_data.id_vstream].cend(); ++it_descriptor)
              {
                double cos_distance = Singleton::cosineDistance(face_descriptor, singleton.id_descriptor_to_data[*it_descriptor]);
                if (cos_distance > max_cos_distance)
                {
                  max_cos_distance = cos_distance;
                  id_descriptor = *it_descriptor;
                }
              }

            //распознавание в специальных группах
            if (task_data.id_sgroup > 0)
            {
              auto it = singleton.id_sgroup_to_id_descriptors.find(task_data.id_sgroup);
              if (it != singleton.id_sgroup_to_id_descriptors.end())
                for (auto id_sg_descriptor : it->second)
                {
                  double cos_distance = Singleton::cosineDistance(face_descriptor, singleton.id_descriptor_to_data[id_sg_descriptor]);
                  if (cos_distance > max_cos_distance)
                  {
                    max_cos_distance = cos_distance;
                    id_descriptor = id_sg_descriptor;
                  }
                }
            } else
            {
              for (const auto& sg_descriptors : singleton.id_sgroup_to_id_descriptors)
              {
                double sg_max_cos_distance = -2.0;
                int id_sg_best_descriptor{};
                for (auto id_sg_descriptor : sg_descriptors.second)
                {
                  double cos_distance = Singleton::cosineDistance(face_descriptor, singleton.id_descriptor_to_data[id_sg_descriptor]);
                  if (cos_distance > sg_max_cos_distance)
                  {
                    sg_max_cos_distance = cos_distance;
                    id_sg_best_descriptor = id_sg_descriptor;
                  }
                }

                if (id_sg_best_descriptor > 0 && sg_max_cos_distance >= conf_tolerance)
                {
                  face_data.back().sg_descriptors[sg_descriptors.first] = {sg_max_cos_distance, id_sg_best_descriptor};
                  has_sgroup_events = true;
                }
              }
            }
          }

          //для теста
          //tt1 = std::chrono::steady_clock::now();
          //cout << "__recognition time: " << std::chrono::duration<double, std::milli>(tt1 - tt0).count() << " ms" << endl;

          face_data.back().cosine_distance = max_cos_distance;

          if (log_level >= LOGS_VERBOSE)
            singleton.addLog(absl::Substitute("Данные самого похожего: cosine_distance = $0; id_descriptor = $1",
              max_cos_distance, id_descriptor));

          if (task_data.task_type == TASK_TEST)
          {
            singleton.addLog(absl::Substitute("Данные для теста: cosine_distance = $0; id_descriptor = $1",
              max_cos_distance, id_descriptor));

            continue;
          }

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

            if (task_data.task_type == TASK_PROCESS_FRAME)
              process_result->id_descriptors.push_back(id_descriptor);
          }

          if (task_data.task_type == TASK_REGISTER_DESCRIPTOR)  //регистрация дескриптора
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

        //для сбора статистики инференса
        if (task_data.task_type == TASK_RECOGNIZE && dnn_stats_data.fd_count > 0)
        {
          lock_guard lock(singleton.mtx_stats);
          singleton.dnn_stats_data[task_data.id_vstream].fd_count += dnn_stats_data.fd_count;
          singleton.dnn_stats_data[task_data.id_vstream].fc_count += dnn_stats_data.fc_count;
          singleton.dnn_stats_data[task_data.id_vstream].fr_count += dnn_stats_data.fr_count;
        }

        if (best_face_index >= 0 && task_data.task_type == TASK_RECOGNIZE)
        {
          singleton.addLog(absl::Substitute("Обнаружены лица детектором: id_vstream = $0", task_data.id_vstream));

          //для теста
          //auto tt000 = std::chrono::steady_clock::now();

          //переключаемся в фоновый режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->background_executor());

          auto [id_log, event_id, screenshot_url] = singleton.addLogFace(task_data.id_vstream, absl::Now(), face_data[best_face_index].id_descriptor,
            face_data[best_face_index].laplacian, face_data[best_face_index].face_rect, image_data);
          saveEventData(event_id, face_data, best_face_index);

          if (id_log > 0 && face_data[best_face_index].id_descriptor > 0)
          {
            //отправляем событие о распознавании лица
            if (!conf_callback_url.empty())
            {
              crow::json::wvalue json_data;
              json_data[API::P_FACE_ID] = face_data[best_face_index].id_descriptor;
              json_data[API::P_LOG_FACES_ID] = id_log;
              cpr::SslOptions ssl_opts = cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false},
                cpr::ssl::VerifyStatus{false});
              cpr::Response response_callback = cpr::Post(cpr::Url{conf_callback_url},
                cpr::Timeout(conf_callback_timeout_ms),
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body(json_data.dump()),
                ssl_opts);
              DeliveryEventResult delivery_result =
                (response_callback.status_code == HTTP_SUCCESS || response_callback.status_code == HTTP_NO_CONTENT)
                  ? DeliveryEventResult::SUCCESSFUL
                  : DeliveryEventResult::ERROR;
              singleton.addLogDeliveryEvent(DeliveryEventType::FACE_RECOGNIZED, delivery_result, task_data.id_vstream,
                face_data[best_face_index].id_descriptor);
              if (delivery_result == DeliveryEventResult::SUCCESSFUL)
                singleton.addLog(
                  absl::Substitute("Отправлено событие о распознавании лица: id_vstream = $0; id_descriptor = $1",
                    task_data.id_vstream, face_data[best_face_index].id_descriptor));
              else
                singleton.addLog(absl::Substitute(
                  "Ошибка! Не удалось отправить событие о распознавании лица: id_vstream = $0; id_descriptor = $1",
                  task_data.id_vstream, face_data[best_face_index].id_descriptor));
            }
          }

          //переключаемся в основной режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());

          //для теста
          //auto tt001 = std::chrono::steady_clock::now();
          //cout << "__write log time: " << std::chrono::duration<double, std::milli>(tt001 - tt000).count() << " ms" << endl;
        }

        //отправляем события о распознавании лиц из специальных групп
        if (has_sgroup_events && task_data.task_type == TASK_RECOGNIZE)
        {
          //переключаемся в фоновый режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->background_executor());

          for (const auto& f : face_data)
            for (const auto& sg_descriptor : f.sg_descriptors)
            {
              auto log_date = absl::Now();
              auto [id_log, event_id, screenshot_url] = singleton.addLogFace(task_data.id_vstream, log_date,
                sg_descriptor.second.id_descriptor, f.laplacian, f.face_rect, image_data);

              if (id_log > 0 && !screenshot_url.empty() && conf_sgroup_callback_url.contains(sg_descriptor.first))
              {
                crow::json::wvalue json_data;
                json_data[API::P_FACE_ID] = sg_descriptor.second.id_descriptor;
                json_data[API::P_SCREENSHOT] = screenshot_url;
                json_data[API::P_DATE] = absl::FormatTime(API::DATETIME_FORMAT, log_date, absl::LocalTimeZone());
                cpr::SslOptions ssl_opts = cpr::Ssl(cpr::ssl::VerifyHost{false}, cpr::ssl::VerifyPeer{false},
                  cpr::ssl::VerifyStatus{false});
                cpr::Response response_callback = cpr::Post(cpr::Url{conf_sgroup_callback_url[sg_descriptor.first]},
                  cpr::Timeout(conf_callback_timeout_ms),
                  cpr::Header{{"Content-Type", "application/json"}},
                  cpr::Body(json_data.dump()),
                  ssl_opts);
                DeliveryEventResult delivery_result =
                  (response_callback.status_code == HTTP_SUCCESS || response_callback.status_code == HTTP_NO_CONTENT)
                    ? DeliveryEventResult::SUCCESSFUL
                    : DeliveryEventResult::ERROR;
                singleton.addLogDeliveryEvent(DeliveryEventType::FACE_RECOGNIZED, delivery_result, task_data.id_vstream,
                  sg_descriptor.second.id_descriptor);
                if (delivery_result == DeliveryEventResult::SUCCESSFUL)
                  singleton.addLog(
                    absl::Substitute("Отправлено событие о распознавании лица: id_sgroup = $0; id_vstream = $1; id_descriptor = $2",
                      sg_descriptor.first, task_data.id_vstream, sg_descriptor.second.id_descriptor));
                else
                  singleton.addLog(absl::Substitute(
                    "Ошибка! Не удалось отправить событие о распознавании лица: id_sgroup = $0; id_vstream = $1; id_descriptor = $2",
                    sg_descriptor.first, task_data.id_vstream, sg_descriptor.second.id_descriptor));
              }
            }

          //переключаемся в основной режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());
        }

        if (task_data.task_type == TASK_REGISTER_DESCRIPTOR)
        {
          if (best_register_index >= 0)
          {
            //все ок, регистрируем дескриптор
            cv::Rect r = enlargeFaceRect(face_data[best_register_index].face_rect, conf_face_enlarge_scale);
            r = r & cv::Rect(0, 0, frame.cols, frame.rows);
            if (face_data[best_register_index].cosine_distance > 0.999)
              response->id_descriptor = face_data[best_register_index].id_descriptor;
            else
            {
              //переключаемся в фоновый режим
              if (!singleton.is_working.load(std::memory_order_relaxed))
                co_return;
              co_await concurrencpp::resume_on(singleton.runtime->background_executor());

              if (task_data.id_vstream > 0)
                response->id_descriptor = singleton.addFaceDescriptor(task_data.id_vstream,
                  face_data[best_register_index].fd, frame(r));
              else
                if (task_data.id_sgroup > 0)
                  response->id_descriptor = singleton.addSGroupFaceDescriptor(task_data.id_sgroup,
                    face_data[best_register_index].fd, frame(r));

              //переключаемся в основной режим
              if (!singleton.is_working.load(std::memory_order_relaxed))
                co_return;
              co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());
            }

            if (response->id_descriptor > 0)
            {
              if (face_data[best_register_index].id_descriptor != response->id_descriptor)
              {
                response->comments = conf_comments_new_descriptor;
                singleton.addLog(absl::Substitute("Создан дескриптор id = $0.", response->id_descriptor));
              } else
              {
                response->comments = conf_comments_descriptor_exists;
                singleton.addLog(absl::Substitute("Дескриптор уже существует id = $0.", response->id_descriptor));
              }
              response->face_image = frame(r).clone();
              response->face_left = face_data[best_register_index].face_rect.x;
              response->face_top = face_data[best_register_index].face_rect.y;
              response->face_width = face_data[best_register_index].face_rect.width;
              response->face_height = face_data[best_register_index].face_rect.height;
            } else
              response->comments = conf_comments_descriptor_creation_error;
          } else
          {
            if (!face_data.empty())
            {
              auto check_index = 0;
              if (!face_data[check_index].is_work_area)
                response->comments = conf_comments_partial_face;
              else
                if (!face_data[check_index].is_frontal)
                  response->comments = conf_comments_non_frontal_face;
                else
                  if (!face_data[check_index].is_non_blurry)
                    response->comments = conf_comments_blurry_face;
                  else
                    if (face_data[check_index].face_class_index != FaceClassIndexes::FACE_NORMAL)
                      response->comments = conf_comments_non_normal_face_class;
                    else
                      response->comments = conf_comments_inference_error;
            } else
              response->comments = conf_comments_no_faces;
          }
        }

        //отрисовка рамки и маркеров, сохранение кадра
        if (task_data.task_type == TASK_TEST)
        {
          for (auto&& f: face_data)
          {
            if (!f.landmarks5.empty())
              for (int k = 0; k < 5; ++k)
                cv::circle(frame, cv::Point(f.landmarks5.at<float>(k, 0), f.landmarks5.at<float>(k, 1)), 1,
                  cv::Scalar(255 * (k * 2 > 2), 255 * (k * 2 > 0 && k * 2 < 8), 255 * (k * 2 < 6)), 4);
            cv::rectangle(frame, f.face_rect, cv::Scalar(0, 200, 0));
          }

          auto frame_indx = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now().time_since_epoch()).count();

          //переключаемся в фоновый режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->background_executor());

          cv::imwrite(singleton.working_path + "/frame_" + std::to_string(frame_indx) + ".jpg", frame);

          //переключаемся в основной режим
          if (!singleton.is_working.load(std::memory_order_relaxed))
            co_return;
          co_await concurrencpp::resume_on(singleton.runtime->thread_pool_executor());

          singleton.addLog(absl::Substitute("Frame index: $0", frame_indx));
        }
      } else
      {
        if (task_data.task_type == TASK_REGISTER_DESCRIPTOR)
        {
          response->id_descriptor = 0;
          response->comments = conf_comments_inference_error;
        }
      }

      if (log_level >= LOGS_VERBOSE || task_data.task_type == TASK_TEST)
        singleton.addLog(absl::Substitute("Конец обработки кадра от видео потока $0.", task_data.id_vstream));
    }

    //продолжаем, если требуется, цепочку заданий
    if (task_data.task_type == TASK_RECOGNIZE)
    {
      bool do_capture = false;

      //scope for lock mutex
      {
        scoped_lock lock(singleton.mtx_capture);
        auto tp_start_motion = singleton.id_vstream_to_start_motion.contains(task_data.id_vstream)
          ? singleton.id_vstream_to_start_motion.at(task_data.id_vstream) : time_point{};
        auto tp_end_motion = singleton.id_vstream_to_end_motion.contains(task_data.id_vstream)
          ? singleton.id_vstream_to_end_motion.at(task_data.id_vstream) : time_point{};
        if (tp_start_motion <= tp_end_motion
          && tp_end_motion + std::chrono::milliseconds(static_cast<int>(1000 * conf_process_frames_interval)) <
          std::chrono::steady_clock::now() || singleton.is_door_open.contains(task_data.id_vstream))
        {
          if (!singleton.is_door_open.contains(task_data.id_vstream))
            singleton.is_capturing.erase(task_data.id_vstream);  //прекращаем захватывать кадры
        } else
          do_capture = true;
      }

      if (do_capture)
      {
        auto new_task_data = TaskData(task_data.id_vstream, TASK_RECOGNIZE);
        new_task_data.delay_s = conf_delay_between_frames;
        singleton.runtime->thread_pool_executor()->submit(processFrame, new_task_data, nullptr, nullptr);
      } else
        singleton.addLog(absl::Substitute("Конец захвата кадров с видео потока $0.", task_data.id_vstream));
    }

    //для теста
    //auto tt01 = std::chrono::steady_clock::now();
    //cout << "__processFrame time: " << std::chrono::duration<double, std::milli>(tt01 - tt00).count() << " ms\n";
  } catch (const concurrencpp::errors::broken_task& e)
  {
    //ничего не делаем
  } catch (const concurrencpp::errors::runtime_shutdown& e)
  {
    //ничего не делаем
  } catch (const std::exception& e)
  {
    singleton.addLog(e.what());
    if (task_data.task_type == TASK_RECOGNIZE)
    {
      scoped_lock lock(singleton.mtx_capture);

      //прекращаем захватывать кадры
      singleton.is_capturing.erase(task_data.id_vstream);
    }
  }
}

concurrencpp::result<void> removeOldLogFaces(bool do_delay)
{
  auto& singleton = Singleton::instance();
  double interval_live;
  double interval_clear_log;

  //scope for lock mutex
  {
    ReadLocker lock(singleton.mtx_task_config);
    interval_live = singleton.getConfigParamValue<double>(CONF_LOG_FACES_LIVE_INTERVAL);
    interval_clear_log = singleton.getConfigParamValue<double>(CONF_CLEAR_OLD_LOG_FACES);
  }

  if (do_delay)
  {
    auto delay_ms = static_cast<int>(interval_clear_log * 3'600'000);
    co_await singleton.runtime->timer_queue()->make_delay_object(std::chrono::milliseconds(delay_ms), singleton.runtime->background_executor());
  }

  singleton.addLog("Удаление устаревших сохраненных логов из таблицы log_faces.");
  DateTime log_date = absl::Now();
  log_date -= absl::Seconds(interval_live * 3'600);

  try
  {
    auto mysql_session = singleton.sql_client->getSession();
    mysql_session.sql(SQL_REMOVE_OLD_LOG_FACES)
      .bind(absl::FormatTime(API::DATETIME_FORMAT_LOG_FACES, log_date, absl::LocalTimeZone()))
      .execute();
  } catch (const mysqlx::Error& err)
  {
    singleton.addLog(absl::Substitute(ERROR_SQL_EXEC_IN_FUNCTION, "SQL_REMOVE_OLD_LOG_FACES", __FUNCTION__));
    singleton.addLog(err.what());
  }

  //удаляем уже ненужные файлы со скриншотами
  singleton.addLog("Удаление устаревших файлов со скриншотами.");
  removeFiles(singleton.screenshot_path, log_date);

  if (singleton.is_working.load(std::memory_order_relaxed))
    singleton.runtime->background_executor()->submit(removeOldLogFaces, true);
}
