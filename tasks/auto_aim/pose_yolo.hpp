#ifndef AUTO_AIM__POSE_YOLO_HPP
#define AUTO_AIM__POSE_YOLO_HPP

#include <opencv2/opencv.hpp>

#include "pose.hpp"

namespace auto_aim
{
class Pose_YOLOBase
{
public:
  virtual std::list<Pose> detect(const cv::Mat & img, int frame_count) = 0;
};

class Pose_YOLO
{
public:
  Pose_YOLO(const std::string & config_path, bool debug = true);

  std::list<Pose> detect(const cv::Mat & img, int frame_count = -1);

private:
  std::unique_ptr<Pose_YOLOBase> yolo_;
};

}  // namespace auto_aim

#endif