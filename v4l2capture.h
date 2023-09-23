/****************************************************************************
*
* Copyright (C) 2019-2023 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@update:  2022.8.13
 *@brief:   该模块使用V4L2的数据结构和接口,采集视频帧,并转换为rgb格式
 *
 *1.该模块使用V4L2的标准流程和接口采集视频帧，使用mmap内存映射的方式实现从内核空间取帧数据到用户空间。
 *2.该模块提供两种取帧方式：
 *  2.1.一种是在类外定时调用指定接口(ioctlDequeueBuffers)取帧，可以自由控制软件的取帧频次，不过有些设
 *备驱动当取帧频次小于硬件帧率时，显示会异常。
 *  2.2另一种是采用select机制自动取帧，该方式在处理性能跟得上的情况下，取帧速率跟帧率一致，每取完一帧数据
 *以信号的形式对外发送。要使用该方式只需在类构造函数中传递useSelect=true参数，内部会自动创建子线程自动
 *完成取帧处理，外部只需绑定相关信号即可。
 *3.该模块目前集成了V4L2_PIX_FMT_YUYV、V4L2_PIX_FMT_NV12、V4L2_PIX_FMT_NV21三种yuv格式到rgb的转换
 *处理，均是使用CCIR 601的转换公式(整形移位)，为了提高处理性能，转换函数已经尽最大可能的进行了优化。
 *
 */
#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QObject>
#include <QThread>
#include <QImage>
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

//缓冲区数量，一般不低于3个，但太多的话按顺序刷新可能会造成视频延迟
#define BUFFER_COUNT 3

class V4L2Capture:public QObject
{
	Q_OBJECT
public:
    //视频采集相关函数
    V4L2Capture(bool useSelect=true,QObject *parent = 0);
    ~V4L2Capture();

    bool openDevice(const char *filename);//打开设备
    void closeDevice();//关闭设备

    /* 以下方法利用V4L2的数据结构结合ioctl()函数实现对视频设备的读写及控制
     * ioctl(int fd,unsigned long cmd,...)函数用于对设备的读取与控制(第三个参数一般涉及数据
     * 传输时使用)，在用户空间使用ioctl系统调用控制设备，用户程序只需要通过命令码cmd告诉驱动程序它想
     * 做什么，具体命令怎么解释和实现由驱动程序(这里就是v4l2驱动程序)的ioctl()函数来实现*/
    //查询设备信息
    void ioctlQueryCapability();//查询设备的基本信息
    void ioctlQueryStd();//查询设备支持的标准
    void ioctlEnumInput();//查询设备的输入
    void ioctlEnumFmt();//查询设备支持的帧格式
    //设置/查询视频流数据
    void ioctlGetStreamParm();//获取视频流参数
    void ioctlSetStreamParm(bool highQuality=false,uint timeperframe=30);//设置视频流参数
    void ioctlGetStreamFmt();//获取视频流格式
    void ioctlSetStreamFmt(uint pixelformat,uint width,uint height);//设置视频流格式
    //初始化帧缓冲区
    void ioctlRequestBuffers();//申请视频帧缓冲区(内核空间)
    bool ioctlMmapBuffers();//映射视频帧缓冲区到用户空间内存
    //帧采集控制
    void ioctlQueueBuffers();//放缓冲帧进输入队列
    bool ioctlDequeueBuffers(uchar *rgbFrameAddr);//从输出队列取缓冲帧
    void ioctlSetStreamSwitch(bool on);//启动/停止视频帧采集

    void unMmapBuffers();//释放视频缓冲区的映射内存

signals:
    //向外发射信号(rgb原始数据流和封装的QImage，按需使用)
    void selectCaptureFrameSig(uchar *rgbFrame);
    void selectCaptureFrameSig(const QImage &rgbImage);

    void selectCaptureSig();//外部调用，用于触发selectCaptureSlot()槽在子线程中执行

public slots:
    void selectCaptureSlot();

private:
    void clearSelectResource();//清理select相关的资源

    /* YUV<---->RGB格式转换常用公式(CCIR 601)如下：
     * 浮点计算(效率低)                                  整形移位:(效率较高)
     *                                                 v=V-128; u=U-128;
     * R=Y+1.403*(V−128)                               R=Y+v+((103*v)>>8)
     * G=Y–0.343*(U–128)–0.714*(V–128)                 G=Y-((88*u)>>8)-((183*v)>>8)
     * B=Y+1.770*(U–128)                               B=Y+u+((197*u)>>8)
     *
     * Y=0.299R+0.587G+0.114B
     * U(Cb)=−0.169R−0.331G+0.500B+128
     * V(Cr)=0.500R−0.419G−0.081B+128
     */
    inline void yuv_to_rgb_shift(int &y,int r_uv,int g_uv,int b_uv,uchar rgb[]);
    void yuyv_to_rgb_shift(uchar *yuyv,uchar *rgb,uint width,uint height);
    void nv12_to_rgb_shift(uchar *nv12,uchar *rgb,uint width,uint height);
    void nv21_to_rgb_shift(uchar *nv21,uchar *rgb,uint width,uint height);


    int cameraFd = -1;//设备文件句柄
    volatile bool isStreamOn = false;//设备采集状态(启/停)
    uint pixelFormat = 0;//采集帧格式
    uint pixelWidth = 720;//像素宽度
    uint pixelHeight = 576;//像素高度

    //select采集
    bool useSelectCapture = false;//是否使用select采集
    QThread *selectThread = NULL;//专用线程
    uchar *selectRgbFrameBuf = NULL;//双缓冲帧
    uchar *selectRgbFrameBuf2 = NULL;

    /*该结构体数组存放每一个被映射的缓冲帧的信息*/
    struct BufferMmap
	{
        uchar * addr = NULL;//缓冲帧映射到内存中的起始地址
        uint length = 0;//缓冲帧映射到内存中的长度
    }bufferMmapPtr[BUFFER_COUNT];

};
#endif //V4L2CAPTURE_H

