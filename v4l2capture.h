/*
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@brief:   该模块使用V4L2的数据结构和接口,采集视频帧,并转换为rgb格式
 */
#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QObject>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>//v4l2的头文件
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define FRAME_WIDTH (640) //帧宽度
#define FRAME_HEIGHT (480) //帧高度

#define BUFFER_COUNT 3//缓冲区数量，一般不低于3个，但太多的话按顺序刷新可能会造成视频延迟
#define VIDEO_DEVICE "/dev/video1"//采集设备对应到linux系统下的文件名

class V4L2Capture:public QObject
{
	Q_OBJECT
public:
    //视频采集相关函数
    V4L2Capture(QObject *parent = 0);
    ~V4L2Capture();

    bool openDevice(const char *filename);//打开设备
    void closeDevice();//关闭设备
    bool initDevice();//初始化设备，用于在视频帧采集前初始化

    /* 以下方法利用V4L2的数据结构结合ioctl()函数实现对视频设备的读写及控制
     * ioctl(int fd,unsigned long cmd,...)函数用于对设备的读取与控制(第三个参数一般涉及数据
     * 传输时使用)，在用户空间使用ioctl系统调用控制设备，用户程序只需要通过命令码cmd告诉
     * 驱动程序它想做什么，具体命令怎么解释和实现由驱动程序(这里就是v4l2驱动程序)的ioctl()
     * 函数来实现*/
    void ioctlQueryCapability();//查询设备的基本信息
    void ioctlQueryStd();//查询设备支持的标准
    void ioctlEnumFmt();//显示设备支持的帧格式
    void ioctlSetStreamParm();//设置视频流参数
    void ioctlSetStreamFmt();//设置视频流格式
    void ioctlRequestBuffers();//申请视频帧缓冲区(内核空间)
    void ioctlMmapBuffers();//映射视频帧缓冲区到用户空间内存
    void ioctlQueueBuffers();//放缓冲帧进输入队列
    void ioctlDequeueBuffers(uchar *rgbFrameAddr);//从输出队列取缓冲帧
    void ioctlSetStreamSwitch(bool on);//启动/停止视频帧采集

    void unMmapBuffers();//释放视频缓冲区的映射内存

    /* yuv<---->rgb格式转换常用公式(CCIR 601)如下：
     * 浮点计算(效率低)                                                整形移位:(效率较高)
     *                                                                              v=V-128; u=U-128;
     * R=Y+1.403*(V−128)                                           R=Y+v+((103*v)>>8)
     * G=Y–0.343*(U–128)–0.714*(V–128)                 G=Y-((88*u)>>8)-((183*v)>>8)
     * B=Y+1.770*(U–128)                                            B=Y+u+((197*u)>>8)
     *
     * Y=0.299R+0.587G+0.114B
     * U(Cb)=−0.169R−0.331G+0.500B+128
     * V(Cr)=0.500R−0.419G−0.081B+128
     */
    void yuyv_to_rgb_shift(uchar *yuyv,uchar *rgb,uint width,uint height);//移位法

public slots:

private:
    int cameraFd;//设备文件句柄
    /*该结构体数组存放每一个被映射的缓冲帧的信息*/
    struct bufferMmap
	{
        uchar * addr;//缓冲帧映射到内存中的起始地址
        uint length;//缓冲帧映射到内存中的长度
    }bufferMmapPtr[BUFFER_COUNT];

    //yuv---rgb转换查表法
    static int table_rv[256];
    static int table_gu[256];
    static int table_gv[256];
    static int table_bu[256];

};
#endif //V4L2CAPTURE_H

