#include "flower_usb.hpp"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

namespace io
{
static int fd = -1;                                            // 串口文件描述符

int usb_open(const char *device)
{
  fd = open(device, O_RDWR | O_NOCTTY);                   // 可读写，且不让该设备成为进程的控制终端
  if (fd == -1)
  {
    perror("open");
    return -1;
  }

  struct termios tty;                                          // 此结构体用于获取当前串口设备的终端属性
  // 用于获取与文件描述符 fd 关联的终端设备的当前属性，并将这些属性填充到 tty 结构体中
  if (tcgetattr(fd, &tty) != 0)
  {
    perror("tcgetattr");
    close(fd);
    fd = -1;
    return -1;
  }
  cfsetospeed(&tty, B9600);                             // 输入波特率为9600
  cfsetispeed(&tty, B9600);                             // 输出波特率为9600

  // 控制模式标志
  tty.c_cflag |= (CLOCAL | CREAD);                            // 启用接收器，忽略调制解调器控制线
  tty.c_cflag &= ~CSIZE;                                      // 清除数据位设置
  tty.c_cflag |= CS8;                                         // 8 位数据
  tty.c_cflag &= ~PARENB;                                     // 无奇偶校验
  tty.c_cflag &= ~CSTOPB;                                     // 1 位停止位
  tty.c_cflag &= ~CRTSCTS;                                    // 禁用硬件流控
  // 本地模式 - 原始输入
  tty.c_lflag &= ~(ICANON | ECHO | ISIG);                     // 非规范模式，无回显，无信号
  // 输入模式 - 禁用软件流控和字符转换
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_iflag &= ~(INLCR | ICRNL | IGNCR);
  // 输出模式 - 原始输出
  tty.c_oflag &= ~OPOST;
  // 设置读取超时：VMIN=0, VTIME=10 表示等待 1 秒（10*0.1秒）后返回，即使没有数据
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1;                                        // 单位：0.1 秒
  // 应用设置
  if (tcsetattr(fd, TCSANOW, &tty) != 0)
  {
    perror("tcsetattr");
    close(fd);
    fd = -1;
    return -1;
  }
  printf("USB device %s opened successfully.\n", device);
  return 0;
}

void usb_close(void)
{
  if (fd != -1)
  {
    close(fd);
    fd = -1;
    printf("USB device closed.\n");
  }
}

// 将用户空间的数据拷贝到内核的串口发送缓冲区，然后由驱动程序通过硬件逐位发送
int usb_send(const char *data, size_t len)
{
  if (fd == -1)
  {
    errno = EBADF;
    return -1;
  }
  ssize_t n = write(fd, data, len);
  if (n < 0)
  {
    perror("write");
    return -1;
  }
  return (int)n;
}

// 接收数据
// 数据从硬件到达后，确实会先存放在内核的串口接收缓冲区中，然后应用程序通过 read 读取
// 这个内核缓冲区的工作方式和你担心的“覆盖”不同——它本质上是一个先进先出的队列，而不是一个只保存最新数据的寄存器
// 当接收速度持续快于读取速度时，缓冲区会逐渐被填满。一旦缓冲区满，再到达的新数据将无处存放，这时通常会丢弃新数据
int usb_recv(char *buf, size_t maxlen)
{
  if (fd == -1)
  {
    errno = EBADF;
    return -1;
  }

  ssize_t n = read(fd, buf, maxlen);
  if (n < 0)
  {
    perror("read");
    return -1;
  }
  return (int)n;
}

}