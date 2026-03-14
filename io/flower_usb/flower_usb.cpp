#include "flower_usb.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cctype>   // for isprint

namespace io
{

Flower_USB::Flower_USB(const char* device_path)
    : fd_(-1), running_(true)
{
  // O_RDWR 读写，O_NOCTTY 防止成为控制终端
  fd_ = open(device_path, O_RDWR | O_NOCTTY);
  if (fd_ < 0)
  {
    std::cerr << "Failed to open " << device_path << ": " << strerror(errno) << std::endl;
    return;
  }

  int flags = fcntl(fd_, F_GETFL, 0);             // 使 read/write 不阻塞
  if (flags != -1)
  {
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);      //
  }

  struct termios tty;
  if (tcgetattr(fd_, &tty) == 0)
  {
    cfmakeraw(&tty);                                    // 禁用所有终端行规则（如 ICANON、ECHO 等），确保收发的是原始字节
    tcsetattr(fd_, TCSANOW, &tty);
  }

  // 启动线程
  sender_thread_ = std::thread(&Flower_USB::sender_loop, this);       // 创建发送线程
  receiver_thread_ = std::thread(&Flower_USB::receiver_loop, this);   // 创建接收线程
}

Flower_USB::~Flower_USB()
{
  stop();
}

void Flower_USB::send(const std::string& data)
{
  if (!running_) return;
  {
    std::lock_guard<std::mutex> lock(send_mutex_);
    send_queue_.push(data);
  }
  send_cond_.notify_one();
}

// 非阻塞地从接收队列取一行（如果有），否则返回空字符串
std::string Flower_USB::try_receive()
{
  std::lock_guard<std::mutex> lock(recv_mutex_);
  if (recv_queue_.empty())                                          // 接收队列为空
  {
    return {};
  }
  std::string line = std::move(recv_queue_.front());
  recv_queue_.pop();
  return line;
}

void Flower_USB::stop()
{
  if (!running_.exchange(false)) return;
  send_cond_.notify_all();
  if (sender_thread_.joinable()) sender_thread_.join();
  if (receiver_thread_.joinable()) receiver_thread_.join();
  if (fd_ >= 0)
  {
    close(fd_);
    fd_ = -1;
  }
}

void Flower_USB::sender_loop()
{
  while (running_)
  {
    std::string data;
    {
      // 创建一个 std::unique_lock 对象 lock，并锁定 send_mutex_
      // unique_lock 比 lock_guard 更灵活，它可以与条件变量配合使用，也允许手动解锁
      std::unique_lock<std::mutex> lock(send_mutex_);

      // 线程会在这里等待，直到满足下面的任一调节条件
      // 发送队列非空（!send_queue_.empty()）：表示有数据需要发送
      // 线程停止标志 running_ 变为 false：表示对象正在被销毁或停止
      // 等待期间：
      // wait 会自动调用 lock.unlock()，让出互斥锁，使其他线程（如 send 函数）可以向队列添加数据
      send_cond_.wait(lock, [this] { return !send_queue_.empty() || !running_; });
      if (!running_ && send_queue_.empty()) break;          // 如果队列空了，并且running_为假即线程停止
      if (!send_queue_.empty())                             // 发现发送队列非空
      {
        data = std::move(send_queue_.front());              // 返回队列头部元素的引用
        send_queue_.pop();                                  // 删除队列的第一个元素
      }
    }

    if (fd_ < 0 || data.empty()) continue;                  // 如果取出的数据不是空

    // 调用 write 将 data 中的内容写入文件描述符 fd_
    ssize_t ret = write(fd_, data.c_str(), data.size());
    if (ret < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // 暂时不可写，放回队尾稍后重试
        std::lock_guard<std::mutex> lock(send_mutex_);
        send_queue_.push(data);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      else
      {
        // 硬错误，丢弃数据
        std::cerr << "USB write error: " << strerror(errno) << std::endl;
      }
    }
    else if ((size_t)ret < data.size())
    {
      // 部分发送，剩余部分重新入队
      std::string remaining = data.substr(ret);
      std::lock_guard<std::mutex> lock(send_mutex_);
      send_queue_.push(remaining);
      send_cond_.notify_one();                            // 立即继续发送剩余部分
    }
  }
}

void Flower_USB::receiver_loop()
{
  const size_t buf_size = 256;
  char buf[buf_size];

  while (running_)
  {
    if (fd_ < 0)
    {
      // 使当前线程阻塞（不占用 CPU）至少 10 毫秒，然后自动恢复执行
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // 调用 read 从文件描述符 fd_ 读取最多 buf_size 字节到缓冲区 buf
    ssize_t n = read(fd_, buf, buf_size);
    if (n > 0)
    {
      recv_buffer_.append(buf, n);                                 // 将新读取的 n 字节追加到成员变量 recv_buffer_ 的末尾
      size_t pos;
      while ((pos = recv_buffer_.find('\n')) != std::string::npos)
      {
        std::string line = recv_buffer_.substr(0, pos);     // 反复从 recv_buffer_ 中查找换行符 \n，每找到一个就提取一行
        if (!line.empty() && line.back() == '\r') line.pop_back(); // 去除可能结尾的 \r
        {
          std::lock_guard<std::mutex> lock(recv_mutex_);       // 加锁后将这一行放入接收队列 recv_queue_
          recv_queue_.push(line);                                  // 这行代码的作用是将解析出的完整数据行放入接收队列中
        }
        recv_buffer_.erase(0, pos + 1);                     // 删除从开头到 \n（包括 \n）的所有字符，即移除已提取的行
      }
    }
    else if (n == 0)
    {
      // 可能设备关闭，短暂休眠
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    else
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
      {
        // 读错误
        std::cerr << "USB read error: " << strerror(errno) << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      else
      {
        // 无数据，短暂休眠
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }
}

} // namespace io