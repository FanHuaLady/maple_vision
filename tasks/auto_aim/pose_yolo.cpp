#include "pose_yolo.hpp"

#include <yaml-cpp/yaml.h>

#include "yolos/pose_yolov8.hpp"

namespace auto_aim
{
Pose_YOLO::Pose_YOLO(const std::string & config_path, bool debug)
{
  auto yaml = YAML::LoadFile(config_path);
  auto yolo_name = yaml["yolo_name"].as<std::string>();                       // 获取配置文件的yolo_name

  if (yolo_name == "yolov8")
  {
    yolo_ = std::make_unique<Pose_YOLOv8>(config_path, debug);
  }
  else
  {
    throw std::runtime_error("Unknown yolo name: " + yolo_name + "!");
  }
}

std::list<Pose> Pose_YOLO::detect(const cv::Mat & img, int frame_count)
{
  return yolo_->detect(img, frame_count);
}

}  // namespace auto_aim