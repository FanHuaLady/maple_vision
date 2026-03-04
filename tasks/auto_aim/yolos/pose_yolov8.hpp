#ifndef AUTO_AIM__POSE_YOLOV8_HPP
#define AUTO_AIM__POSE_YOLOV8_HPP

#include <list>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "tasks/auto_aim/pose.hpp"
// #include "tasks/auto_aim/detector.hpp"
#include "tasks/auto_aim/pose_yolo.hpp"

extern "C" {
#include "rknn_api.h"
}

namespace auto_aim
{
class Pose_YOLOv8 : public Pose_YOLOBase                              // 假设基类存在，否则可先去掉继承
{
public:
  Pose_YOLOv8(const std::string & config_path, bool debug);

  std::list<Pose> detect(const cv::Mat & bgr_img, int frame_count);

private:
  std::string model_path_;
  std::string save_path_;
  bool debug_;
  bool use_roi_;
  cv::Rect roi_;
  cv::Point2f offset_;

  // RKNN相关成员
  rknn_context ctx_;
  rknn_input_output_num io_num_;
  std::vector<uint8_t> model_data_;

  rknn_input rknn_input_;
  rknn_output* rknn_outputs_;                                       // 输出数组，长度为 io_num_.n_output
  cv::Mat input_buffer_;                                            // 640x640的输入图像缓冲区

  // 模型参数
  const int img_size_ = 640;
  const float conf_threshold_ = 0.25;
  const float nms_threshold_ = 0.45;
  const int num_classes_ = 80;                                      // COCO类别数，若只检测人可改为1，但后处理需调整

  // 辅助函数
  void letterbox(const cv::Mat &src, cv::Mat &dst, double &scale, int &pad_left, int &pad_top);
  std::list<Pose> parse_outputs(double scale, int pad_left, int pad_top,
                                const cv::Mat &raw_img, int frame_count);
  void draw_detections(const cv::Mat &img, const std::list<Pose> &poses, int frame_count) const;
};

}  // namespace auto_aim

#endif  // AUTO_AIM__HUMAN_YOLOV8_HPP