/****************************************************************************
*
* Copyright (C) 2019-2025 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
#include "videodisplaywidget.h"
#include <QGridLayout>
#include <QTime>
#include <QDebug>

//采集设备对应到linux系统下的文件名
#define VIDEO_DEVICE "/dev/video1"
//采集帧宽高
#define FRAME_WIDTH (1280)
#define FRAME_HEIGHT (720)

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent) :
    QWidget(parent)
{
    //展示视频画面
#ifdef USE_YUV_RENDERING_WIDGET
    videoOutput = new OpenGLWidget(V4L2_PIX_FMT_NV21,FRAME_WIDTH,FRAME_HEIGHT,this);
    videoOutput->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    //videoOutput->setFixedSize(FRAME_WIDTH,FRAME_HEIGHT);
    //videoOutput->readYuvFileTest("./video/nv21_854x480.yuv",V4L2_PIX_FMT_NV21,FRAME_WIDTH,FRAME_HEIGHT);
#else
    videoOutput = new PixmapWidget(this);
    videoOutput->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    //videoOutput->setFixedSize(FRAME_WIDTH,FRAME_HEIGHT);
    //videoOutput->readYuvFileTest("./video/nv21_854x480.yuv",V4L2_PIX_FMT_NV21,FRAME_WIDTH,FRAME_HEIGHT);
#endif

    //采集开始/结束按钮
    captureBtn = new QPushButton(this);
    captureBtn->setFixedHeight(80);
    captureBtn->setText("start capture");
    connect(captureBtn,SIGNAL(clicked()),this,SLOT(captureBtnClickedSlot()));
    //保存图片按钮
    saveImageBtn = new QPushButton(this);
    saveImageBtn->setFixedHeight(80);
    saveImageBtn->setText("save image");
    connect(saveImageBtn,SIGNAL(clicked()),this,SLOT(saveImageBtnClickedSlot()));
    //退出程序按钮
    quitBtn = new QPushButton(this);
    quitBtn->setFixedHeight(80);
    quitBtn->setText("quit");
    connect(quitBtn,&QPushButton::clicked,this,&VideoDisplayWidget::close);

    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setMargin(1);
    gridLayout->addWidget(videoOutput,0,0,3,7);
    gridLayout->addWidget(captureBtn,0,7,1,1);
    gridLayout->addWidget(saveImageBtn,1,7,1,1);
    gridLayout->addWidget(quitBtn,2,7,1,1);
    this->resize(1024,600);

#ifdef USE_SELECT_CAPTURE
    initSelectCapture();
#else
    initTimerCapture();
#endif
}

VideoDisplayWidget::~VideoDisplayWidget()
{
#ifdef USE_SELECT_CAPTURE
    if(v4l2Capture)
    {
        delete v4l2Capture;
    }
#else
    if(timerRgbFrameBuf)
    {
        free(timerRgbFrameBuf);
    }
#endif
}
#ifdef USE_SELECT_CAPTURE
/*
 *@brief:  select自动采集相关初始化
 *@author: 缪庆瑞
 *@date:   2022.08.16
 */
void VideoDisplayWidget::initSelectCapture()
{
    v4l2Capture = new V4L2Capture(true,0);//视频采集对象
    initV4l2CaptureDevice();
#ifdef USE_YUV_RENDERING_WIDGET
    connect(v4l2Capture,SIGNAL(captureOriginFrameSig(uchar**)),videoOutput,SLOT(updateV4l2FrameSlot(uchar**)));
#else
    connect(v4l2Capture,&V4L2Capture::captureRgb24FrameSig,
            this,[this](uchar *rgb24Frame){
        if(this->isVisible())
        {
            QImage selectImage(rgb24Frame,FRAME_WIDTH,FRAME_HEIGHT,FRAME_WIDTH*3,QImage::Format_RGB888);
            videoOutput->setPixmap(QPixmap::fromImage(selectImage));//屏幕显示
            videoOutput->update();//刷新显示
            if(isSaveImage)//保存图片
            {
                isSaveImage = false;
                selectImage.save(QTime::currentTime().toString("HHmmsszzz")+".png",nullptr,100);
            }
        }
    });
#endif
}
#else
/*
 *@brief:  定时采集相关初始化
 *@author: 缪庆瑞
 *@date:   2022.08.16
 */
void VideoDisplayWidget::initTimerCapture()
{
    v4l2Capture = new V4L2Capture(false,this);//视频采集对象
    initV4l2CaptureDevice();

#ifdef USE_YUV_RENDERING_WIDGET
    uchar *orignFrameAddr[8];
    timer = new QTimer(this);//定时获取视频帧
    connect(timer,&QTimer::timeout,this,[this,&orignFrameAddr](){
        if(this->isVisible())//仅当前界面被展示才获取界面
        {
            v4l2Capture->ioctlDequeueBuffers(nullptr,orignFrameAddr);
            videoOutput->updateYuvFrameSlot(orignFrameAddr);
        }
    });
#else
    timerRgbFrameBuf = (uchar *)malloc(FRAME_WIDTH*FRAME_HEIGHT*3);//为图像帧分配内存空间
    timerImage = QImage(timerRgbFrameBuf,FRAME_WIDTH,FRAME_HEIGHT,FRAME_WIDTH*3,QImage::Format_RGB888);//根据内存空间创建image图像
    timer = new QTimer(this);//定时获取视频帧
    connect(timer,&QTimer::timeout,this,[this](){
        if(this->isVisible())//仅当前界面被展示才获取界面
        {
            v4l2Capture->ioctlDequeueBuffers(timerRgbFrameBuf);//获取一帧RGB格式图片流
            videoOutput->setPixmap(QPixmap::fromImage(timerImage));//屏幕显示
            videoOutput->update();//刷新显示
            if(isSaveImage)//保存图片
            {
                isSaveImage = false;
                timerImage.save(QTime::currentTime().toString("HHmmsszzz")+".png",nullptr,100);
            }
        }
    });
#endif
}
#endif
/*
 *@brief:  开始采集/停止采集 按钮响应槽
 *@author: 缪庆瑞
 *@date:   2022.08.19
 */
void VideoDisplayWidget::captureBtnClickedSlot()
{
#ifdef USE_SELECT_CAPTURE
    if(captureBtn->text() == "start capture")
    {
        captureBtn->setText("stop capture");
        v4l2Capture->ioctlSetStreamSwitch(true);
#ifdef USE_YUV_RENDERING_WIDGET
        emit v4l2Capture->selectCaptureSig(false,true);
#else
        emit v4l2Capture->selectCaptureSig(true,false);
#endif
    }
    else
    {
        captureBtn->setText("start capture");
        v4l2Capture->ioctlSetStreamSwitch(false);
        //有些平台驱动有问题，停止采集后需要重置设备，否则再次开启采集，画面显示会异常
        v4l2Capture->resetDevice();
    }
#else
    if(captureBtn->text() == "start capture")
    {
        captureBtn->setText("stop capture");
        v4l2Capture->ioctlSetStreamSwitch(true);//开始视频采集
        timer->start(30);
    }
    else
    {
        captureBtn->setText("start capture");
        v4l2Capture->ioctlSetStreamSwitch(false);
        timer->stop();
    }
#endif
}
/*
 *@brief:  保存当前图片
 *@author: 缪庆瑞
 *@date:   2022.08.22
 */
void VideoDisplayWidget::saveImageBtnClickedSlot()
{
    isSaveImage = true;
}
/*
 *@brief:  初始化视频采集设备
 *注:该函数内部调用V4L2Capture类的各种ioctl*()接口，属于采集设备初始化的标准流程，使用者只需根据
 *具体的采集设备稍作调整，传递相关参数即可。
 *@author: 缪庆瑞
 *@date:   2022.08.13
 *@return: bool:true=成功  false=失败
 */
bool VideoDisplayWidget::initV4l2CaptureDevice()
{
    qDebug()<<"initDevice-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    if(!v4l2Capture->openDevice(VIDEO_DEVICE,false))
    {
        return false;
    }
    /*检查设备的一些基本信息(可选项)*/
    v4l2Capture->ioctlQueryCapability();
    v4l2Capture->ioctlQueryStd();
    v4l2Capture->ioctlEnumInput();
    v4l2Capture->ioctlSetInput(0);
    v4l2Capture->ioctlEnumFmt();
    /*设置采集数据的一些参数及格式(必选项)*/
    v4l2Capture->ioctlSetStreamParm(2,25);//T517驱动传递2
    v4l2Capture->ioctlSetStreamFmt(V4L2_PIX_FMT_NV21,FRAME_WIDTH,FRAME_HEIGHT);
    /*设置完参数后稍作延时，这个很关键，否则可能会出现进程退出的情况*/
    usleep(100000);
    /*申请视频帧缓冲区,对帧缓冲区进行内存映射(必选项)*/
    v4l2Capture->ioctlRequestBuffers();
    if(!v4l2Capture->ioctlMmapBuffers())
    {
        return false;
    }
    /*查询设置的参数是否生效*/
    v4l2Capture->ioctlGetStreamParm();
    v4l2Capture->ioctlGetStreamFmt();
    qDebug()<<"initDevice-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    return true;
}
