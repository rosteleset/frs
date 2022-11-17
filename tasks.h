#pragma once

#include "frs_api.h"
#include "singleton.h"

extern concurrencpp::result<void> checkMotion(TaskData task_data);
extern concurrencpp::result<void> processFrame(TaskData task_data,
  std::shared_ptr<RegisterDescriptorResponse> response,
  std::shared_ptr<ProcessFrameResult> process_result);

extern concurrencpp::result<void> removeOldLogFaces(bool do_delay);
