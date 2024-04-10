/****************************************************************************
*
* Copyright (C) 2019-2024 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@update:  2024.3.6
 *@brief:   该模块使用V4L2的数据结构和接口,采集视频帧,并根据需求选择是否进行软解码转换
 *
 *1.该模块使用V4L2的标准流程和接口采集视频帧，使用mmap内存映射的方式实现从内核空间取帧数据到用户空间。
 *2.该模块提供两种取帧方式：
 *  2.1.一种是在类外定时调用指定接口(ioctlDequeueBuffers)取帧，可以自由控制软件的取帧频次，不过有些设
 *备驱动当取帧频次小于硬件帧率时，显示会异常。
 *  2.2另一种是采用select机制自动取帧，该方式在处理性能跟得上的情况下，取帧速率跟帧率一致，每取完一帧数据
 *以信号的形式对外发送。要使用该方式只需在类构造函数中传递useSelect=true参数，内部会自动创建子线程自动
 *完成取帧处理，外部只需绑定相关信号即可。
 */
#ifndef V4L2CAPTURE_H
#define V4L2CAPTURE_H

#include <QObject>
#include <QThread>
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

    bool openDevice(const char *filename,bool isNonblock);//打开设备
    void closeDevice();//关闭设备

    /* 以下方法利用V4L2的数据结构结合ioctl()函数实现对视频设备的读写及控制
     * ioctl(int fd,unsigned long cmd,...)函数用于对设备的读取与控制(第三个参数一般涉及数据
     * 传输时使用)，在用户空间使用ioctl系统调用控制设备，用户程序只需要通过命令码cmd告诉驱动程序它想
     * 做什么，具体命令怎么解释和实现由驱动程序(这里就是v4l2驱动程序)的ioctl()函数来实现*/
    //查询设备信息
    void ioctlQueryCapability();//查询设备的基本信息
    void ioctlQueryStd();//查询设备支持的标准
    void ioctlEnumInput();//查询设备支持的输入
    void ioctlEnumFmt();//查询设备支持的帧格式
    //设置/查询视频流数据
    void ioctlSetInput(int inputIndex);//设置当前设备输入
    void ioctlGetStreamParm();//获取视频流参数
    void ioctlSetStreamParm(uint captureMode,uint timeperframe=30);//设置视频流参数
    void ioctlGetStreamFmt();//获取视频流格式
    void ioctlSetStreamFmt(uint pixelformat,uint width,uint height);//设置视频流格式
    //初始化帧缓冲区
    void ioctlRequestBuffers();//申请视频帧缓冲区(内核空间)
    bool ioctlMmapBuffers();//映射视频帧缓冲区到用户空间内存
    //帧采集控制
    void ioctlSetStreamSwitch(bool on);//启动/停止视频帧采集
    bool ioctlDequeueBuffers(uchar *rgb24FrameAddr,uchar *originFrameAddr[]=NULL);//从输出队列取缓冲帧

signals:
    //向外发射采集到的帧数据信号
    void captureOriginFrameSig(uchar *originFrame[]);//原始数据帧(pixelFormat,长度针对多平面类型的数量，单平面为1)
    void captureRgb24FrameSig(uchar *rgb24Frame);//转换后的rgb24数据帧

    //外部调用，用于触发selectCaptureSlot()槽在子线程中执行
    void selectCaptureSig(bool needRgb24Frame,bool needOriginFrame);

public slots:
    void selectCaptureSlot(bool needRgb24Frame,bool needOriginFrame);

private:
    void ioctlQueueBuffers();//放缓冲帧进输入队列，在开始采集时会调用一次
    void unMmapBuffers();//释放视频缓冲区的映射内存
    void clearSelectResource();//清理select相关的资源

    /*采集设备参数*/
    int cameraFd = -1;//设备文件句柄
    int v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;//采集帧类型(目前主要分单plane和多plane，根据查询的v4l2_capability自动设置)
    int planes_num = 1;//平面数，针对多平面采集帧格式(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    volatile bool isStreamOn = false;//设备采集状态(启/停)
    uint pixelFormat = 0;//采集帧格式
    uint pixelWidth = 720;//像素宽度
    uint pixelHeight = 576;//像素高度

    /*select采集*/
    bool useSelectCapture = false;//是否使用select采集
    QThread *selectThread = NULL;//专用线程
    uchar *selectRgbFrameBuf = NULL;//双缓冲帧
    uchar *selectRgbFrameBuf2 = NULL;

    /*缓存帧内存映射信息*/
    struct BufferMmap//单平面
	{
        uchar * addr = NULL;//缓冲帧映射到内存中的起始地址
        uint length = 0;//缓冲帧映射到内存中的长度
    }bufferMmapPtr[BUFFER_COUNT];
    struct BufferMmapMplane//多平面
    {
        uchar * addr[VIDEO_MAX_PLANES] = {NULL};//缓冲帧(每个平面)映射到内存中的起始地址
        uint length[VIDEO_MAX_PLANES] = {0};//缓冲帧(每个平面)映射到内存中的长度
    }bufferMmapMplanePtr[BUFFER_COUNT];

};
#endif //V4L2CAPTURE_H

