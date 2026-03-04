#include "pose.hpp"
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>

namespace auto_aim
{

Pose::Pose(const cv::Rect& box, float conf, int cls)
  : bbox(box), confidence(conf), class_id(cls)
{
  // 根据边界框计算中心点
  center.x = box.x + box.width / 2.0f;
  center.y = box.y + box.height / 2.0f;
}

}