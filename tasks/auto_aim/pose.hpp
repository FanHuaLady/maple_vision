#ifndef AUTO_AIM__POSE_HPP
#define AUTO_AIM__POSE_HPP

#include <Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <array>          // 用于固定大小的数组

namespace auto_aim
{

// 人体姿态数据结构
struct Pose
{
  cv::Rect bbox;          // 边界框（在原始图像坐标系下）
  cv::Point2f center;     // 中心点（bbox的中心）
  float confidence;       // 检测置信度
  int class_id;           // 类别ID（COCO中人的ID为0）

  Pose(const cv::Rect& box, float conf, int cls = 0);
};

}  // namespace auto_aim

#endif  // AUTO_AIM__POSE_HPP