#include "io/camera.hpp"

#include <opencv2/opencv.hpp>

#include "tools/exiter.hpp"
#include "tools/logger.hpp"
#include "tools/math_tools.hpp"
#include "tasks/auto_aim/pose_yolo.hpp"   // 你的检测类头文件（根据实际情况调整）

const std::string keys =
  "{help h usage ? |                     | 输出命令行参数说明}"
  "{config-path c  | configs/pose.yaml    | yaml配置文件路径 }"
  "{d display      | true                 | 显示视频流       }";

int main(int argc, char * argv[])
{
  cv::CommandLineParser cli(argc, argv, keys);
  if (cli.has("help"))
  {
    cli.printMessage();
    return 0;
  }

  tools::Exiter exiter;

  auto config_path = cli.get<std::string>("config-path");
  auto display = cli.has("display");

  // 初始化摄像头（根据配置文件）
  io::Camera camera(config_path);
  // 初始化人体检测器（根据配置文件）
  auto_aim::Pose_YOLO yolo(config_path);

  cv::Mat img;
  std::chrono::steady_clock::time_point timestamp;
  auto last_stamp = std::chrono::steady_clock::now();

  for (int frame_count = 0; !exiter.exit(); frame_count++)
  {
    // 读取一帧
    camera.read(img, timestamp);
    if (img.empty())
    {
      tools::logger()->warn("Empty frame received, skipping...");
      continue;
    }

    // 执行检测
    auto detections = yolo.detect(img, frame_count);

    // 计算帧率
    auto dt = tools::delta_time(timestamp, last_stamp);
    last_stamp = timestamp;

    // 日志输出
    tools::logger()->info("Frame {}: {:.2f} fps, {} detections",
                          frame_count, 1.0 / dt, detections.size());

    if (display)
    {
      // cv::imshow("Human Detection", img);
      if (cv::waitKey(1) == 'q')
        break;
    }
  }

  return 0;
}