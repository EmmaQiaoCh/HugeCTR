#include <vector>
#include <cstdlib>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>
#include <mutex>

#include <cuda.h>
#include <cuda_runtime.h>

#include <profiler.hpp>
#include <common.hpp>
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace HugeCTR {

  Profiler::GPUTimer::GPUTimer(cudaStream_t stream) {
    stream_ = stream;
    cudaEventCreate(start_);
    cudaEventCreate(stop_);
  }

  Profiler::GPUTimer::~GPUTimer() {
    cudaEventDestroy(*start_);
    cudaEventDestroy(*stop_);
  }

  void Profiler::GPUTimer::event_start() {
    cudaEventRecord(*start_, stream_);
  }

  void Profiler::GPUTimer::event_stop() {
    cudaEventRecord(*stop_, stream_);
  }

  float Profiler::GPUTimer::get_result() {
    float elapsed_time;
    cudaEventElapsedTime(&elapsed_time, *start_, *stop_);
    return elapsed_time;
  }

  void Profiler::initialize(const char* schedule_file) {
    // read from a schedule file, schedule file format:
    //    warmup_iterations
    //    event_name_1, iteration1,
    //    event_name_2, iteration2
    //    ...

    // TBD how to get host_name in CPP ?

    MESSAGE_(std::string("Profiler initializing using ") + schedule_file + " ...");
    std::ifstream schedule_f(schedule_file);
    int line_no = 0;
    for (std::string line; getline(schedule_f, line);) {
        if (line_no) {
          auto splited = split_string(line, ' ');
          scheduled_events_.push_back(std::make_pair(splited[0], unsigned(std::stoi(splited[1]))));
        } else {
          warmup_iterations_ = std::stoi(line);
        }
        line_no++;
    }
    current_iteration_ = 0;
    current_schedule_idx_ = 0;
  }

  void Profiler::iter_start() {
    map_event_id_current_gpu_timer_.clear();
  }

  void Profiler::iter_end() {
    if (current_iteration_ > warmup_iterations_) {
      // get result;
      for(auto& it : map_event_id_current_gpu_timer_) {
        float result = it.second->get_result();
        events_[it.first]->measured_times.push_back(result);
      }
      current_schedule_idx_++;
    }
    if (current_schedule_idx_ >= scheduled_events_.size()) {
        auto result_file = write_result(std::getenv("PROFILING_RESULT_DIR"));
        MESSAGE_(std::string("Profiling complete! Result file is writing to ") + result_file + ". Program exit.");
        std::exit(0);
    }
    current_iteration_++;
  }

  void Profiler::record_event(const char* event_label_char, cudaStream_t stream) {
    // event_label is xxx.xxx.start or xxx.xxx.end, parse suffix out of it
    auto event_label = std::string(event_label_char);
    int dot_pos = event_label.find_last_of(std::string("."));
    std::string event_type = event_label.substr(dot_pos + 1);
    if (event_type != "start" || event_type != "stop") {
      throw internal_runtime_error(HugeCTR::Error_t::UnspecificError, \
      std::string("Invalid event name. Should end with .start or .stop"));
    }
    std::string event_name = event_label.substr(0, dot_pos);

    if (current_iteration_ <= warmup_iterations_) {
      // parse the event label, register it and create resources.
      mtx_.lock();

      auto it = scheduled_events_.begin();
      for (; it != scheduled_events_.end(); it++) {
        if (it->first == event_name) { break;}
      }
      if (it == scheduled_events_.end()) { return; }

      auto map_iter = map_stream_gpu_timer_.find(stream);
      unsigned int stream_id = map_stream_id_[stream];

      if(map_iter == map_stream_gpu_timer_.end()) {
        auto gpu_timer = std::make_shared<GPUTimer>(stream);
        map_stream_gpu_timer_[stream] = gpu_timer;
        map_iter = map_stream_gpu_timer_.end();
        stream_id = distance(map_stream_gpu_timer_.begin(), map_iter);
        map_stream_id_[stream] = stream_id;
      }

      // get device id from stream
      unsigned int device_id = get_device_id(stream);

      if (event_type == "start") {
        // create new event
        auto gpu_event = new GPUEvent;
        gpu_event->name = event_name;
        gpu_event->start_index = events_num_;
        gpu_event->end_index = 0; // wait for stop event to set,
        gpu_event->measured_times = std::vector<float>();
        gpu_event->device_id = device_id;
        gpu_event->stream_id = stream_id;

        events_.push_back(std::shared_ptr<Event>(static_cast<Event*>(gpu_event)));
      } else { // event_name == "end"
        // only update the end_index
        events_[find_event(event_name, stream)]->end_index = events_num_;
      }
      events_num_++;
      MESSAGE_(std::string("Parsed a new GPU event ") + event_label + " on stream " + "stream_id");
      mtx_.unlock();
    } else {
      if (scheduled_events_[current_schedule_idx_].first != event_name || \
          current_iteration_ != scheduled_events_[current_schedule_idx_].second) { return; }
      MESSAGE_("Timing on event " + event_label);
      auto gpu_timer = map_stream_gpu_timer_[stream];
      if (event_type == "start") {
        gpu_timer->event_start();
      } else {
        gpu_timer->event_stop();
        int event_idx = find_event(event_name, stream);
        if (event_idx < 0) {
          throw internal_runtime_error(HugeCTR::Error_t::UnspecificError, \
            std::string("Current event ") + event_name + std::string(" not registered!"));
        }
        mtx_.lock();
        map_event_id_current_gpu_timer_[event_idx] = gpu_timer;
        mtx_.unlock();
      }
    }
  }

  int Profiler::find_event(std::string& event_name, cudaStream_t stream) {
    for (int i = 0; unsigned(i) < events_.size(); i++) {
      if (events_[i]->name == event_name && static_cast<GPUEvent*>(events_[i].get())->stream_id == map_stream_id_[stream]) {
        return i;
      }
    }
    return -1;
  }

  std::string Profiler::write_result(const char* result_dir) {
    // TBD dump events_ to json file
    json result = json::array();
    for (auto& event_p : events_) {
      GPUEvent* gep = static_cast<GPUEvent*>(event_p.get());
      json j;
      j["name"] = gep->name;
      j["start_index"] = gep->start_index;
      j["end_index"] = gep->end_index;
      j["measured_times"] = gep->measured_times;
      j["device_id"] = gep->device_id;
      j["stream_id"] = gep->stream_id;

      result.push_back(j);
    }
    std::string result_jstring = result.dump();
    std::ofstream outfile;
    std::string result_file = std::string(result_dir) + "/" + "prof_result.json";
    outfile.open(result_file);
    outfile << result_jstring;
    outfile.close();
    return result_file;
  }

  // A global variable
  Profiler global_profiler;

}  // namespace HugeCTR
