/****************************************************************************
*
* Copyright (C) 2019-2023 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
#include "videodisplaywidget.h"
#include <QGridLayout>
#include <QTime>
#include <QDebug>

//采集设备对应到linux系统下的文件名
#define VIDEO_DEVICE "/dev/video5"
//采集帧宽高
#define FRAME_WIDTH (720)
#define FRAME_HEIGHT (480)

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent) :
    QWidget(parent)
{
    //展示视频画面
#ifdef USE_OPENGL_DISPLAY
    videoOutput = new PixmapWidget(this);
#else
    videoOutput = new QLabel(this);
    videoOutput->setStyleSheet("border:2px solid black;");
    videoOutput->setScaledContents(true);//按照label的大小放缩视频
#endif
    videoOutput->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    videoOutput->setFixedSize(FRAME_WIDTH,FRAME_HEIGHT);

    //采集开始/结束按钮
    captureBtn = new QPushButton(this);
    captureBtn->setText("start capture");
    connect(captureBtn,SIGNAL(clicked()),this,SLOT(captureBtnClickedSlot()));
    //保存图片按钮
    saveImageBtn = new QPushButton(this);
    saveImageBtn->setText("save image");
    connect(saveImageBtn,SIGNAL(clicked()),this,SLOT(saveImageBtnClickedSlot()));
    //退出程序按钮
    quitBtn = new QPushButton(this);
    quitBtn->setText("quit");
    connect(quitBtn,&QPushButton::clicked,this,&VideoDisplayWidget::close);

    QGridLayout *gridLayout = new QGridLayout(this);
    gridLayout->setMargin(1);
    gridLayout->addWidget(videoOutput,0,0,3,5);
    gridLayout->addWidget(captureBtn,0,5,1,1);
    gridLayout->addWidget(saveImageBtn,1,5,1,1);
    gridLayout->addWidget(quitBtn,2,5,1,1);
    this->resize(1024,600);

    //定时采集或者select采集选一种
    //initTimerCapture();
    initSelectCapture();

}

VideoDisplayWidget::~VideoDisplayWidget()
{
    if(useSelectCapture && v4l2Capture)
    {
        delete v4l2Capture;
    }
    if(timerRgbFrameBuf)
    {
        free(timerRgbFrameBuf);
    }
}
/*
 *@brief:  定时采集相关初始化
 *@author: 缪庆瑞
 *@date:   2022.08.16
 */
void VideoDisplayWidget::initTimerCapture()
{
    useSelectCapture = false;

    v4l2Capture = new V4L2Capture(useSelectCapture,this);//视频采集对象
    initV4l2CaptureDevice();

    timerRgbFrameBuf = (uchar *)malloc(FRAME_WIDTH*FRAME_HEIGHT*3);//为图像帧分配内存空间
    timerImage = QImage(timerRgbFrameBuf,FRAME_WIDTH,FRAME_HEIGHT,QImage::Format_RGB888);//根据内存空间创建image图像
    timer = new QTimer(this);//定时获取视频帧
    connect(timer,SIGNAL(timeout()),this,SLOT(timerCaptureFrameSlot()));

}
/*
 *@brief:  select自动采集相关初始化
 *@author: 缪庆瑞
 *@date:   2022.08.16
 */
void VideoDisplayWidget::initSelectCapture()
{
    useSelectCapture = true;

    v4l2Capture = new V4L2Capture(useSelectCapture,0);//视频采集对象
//    connect(v4l2Capture,SIGNAL(selectCaptureFrameSig(uchar *)),
//            this,SLOT(selectCaptureFrameSlot(uchar*)));
    connect(v4l2Capture,SIGNAL(selectCaptureFrameSig(const QImage&)),
            this,SLOT(selectCaptureFrameSlot(const QImage&)));
    initV4l2CaptureDevice();
}
/*
 *@brief:  开始采集/停止采集 按钮响应槽
 *@author: 缪庆瑞
 *@date:   2022.08.19
 */
void VideoDisplayWidget::captureBtnClickedSlot()
{
    if(useSelectCapture)
    {
        if(captureBtn->text() == "start capture")
        {
            captureBtn->setText("stop capture");
            v4l2Capture->ioctlSetStreamSwitch(true);
            emit v4l2Capture->selectCaptureSig();
        }
        else
        {
            captureBtn->setText("start capture");
            v4l2Capture->ioctlSetStreamSwitch(false);
        }
    }
    else
    {
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
    }
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
 *@brief:  采集定时器超时响应槽
 *@author: 缪庆瑞
 *@date:   2022.08.19
 */
void VideoDisplayWidget::timerCaptureFrameSlot()
{
    //qDebug()<<"timerCaptureFrameSlot-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
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
}
/*
 *@brief:  select自动采集上传的rgb帧信号
 *@author: 缪庆瑞
 *@date:   2022.08.16
 *@param:   rgbFrame:原始rgb数据
 */
void VideoDisplayWidget::selectCaptureFrameSlot(uchar *rgbFrame)
{
    //qDebug()<<"selectCaptureFrameSlot-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    if(this->isVisible())
    {
        QImage selectImage(rgbFrame,FRAME_WIDTH,FRAME_HEIGHT,QImage::Format_RGB888);
        videoOutput->setPixmap(QPixmap::fromImage(selectImage));//屏幕显示
        videoOutput->update();//刷新显示
        if(isSaveImage)//保存图片
        {
            isSaveImage = false;
            selectImage.save(QTime::currentTime().toString("HHmmsszzz")+".png",nullptr,100);
        }
    }
}
/*
 *@brief:  select自动采集上传的QImage信号
 *@author: 缪庆瑞
 *@date:   2022.08.16
 *@param:   rgbImage:封装好的QImage
 */
void VideoDisplayWidget::selectCaptureFrameSlot(const QImage &rgbImage)
{
    //qDebug()<<"selectCaptureFrameSlot-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    if(this->isVisible())
    {
        videoOutput->setPixmap(QPixmap::fromImage(rgbImage));//屏幕显示
        videoOutput->update();//刷新显示
        if(isSaveImage)//保存图片
        {
            isSaveImage = false;
            rgbImage.save(QTime::currentTime().toString("HHmmsszzz")+".png",nullptr,100);
        }
    }
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
    if(!v4l2Capture->openDevice(VIDEO_DEVICE))
    {
        return false;
    }
    /*检查设备的一些基本信息(可选项)*/
    v4l2Capture->ioctlQueryCapability();
    v4l2Capture->ioctlQueryStd();
    v4l2Capture->ioctlEnumInput();
    v4l2Capture->ioctlEnumFmt();
    /*设置采集数据的一些参数及格式(必选项)*/
    v4l2Capture->ioctlSetStreamParm(false,25);
    v4l2Capture->ioctlSetStreamFmt(V4L2_PIX_FMT_NV12,FRAME_WIDTH,FRAME_HEIGHT);
    v4l2Capture->ioctlGetStreamParm();
    v4l2Capture->ioctlGetStreamFmt();
    /*设置完参数后稍作延时，这个很关键，否则可能会出现进程退出的情况*/
    usleep(100000);
    /*申请视频帧缓冲区，并进行相关处理(必选项)*/
    v4l2Capture->ioctlRequestBuffers();
    if(!v4l2Capture->ioctlMmapBuffers())
    {
        return false;
    }
    qDebug()<<"initDevice-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    return true;
}
