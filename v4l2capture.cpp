/****************************************************************************
*
* Copyright (C) 2019-2023 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
#include "v4l2capture.h"
#include <QTime>
#include <QDebug>

/*
 *@brief:   构造函数
 *@author:  缪庆瑞
 *@date:   2022.08.16
 *@param:  useSelect:true=使用select机制取缓冲帧，会使用单独的子线程  false=需要类外主动调用接口取缓冲帧
 *@param:  parent:父对象，当需要使用moveToThread()时，必须为0
 */
V4L2Capture::V4L2Capture(bool useSelect, QObject *parent):
    QObject(parent),useSelectCapture(useSelect)
{
    if(useSelectCapture)
    {
        selectThread = new QThread(this);
        this->moveToThread(selectThread);
        selectThread->start();
        connect(this,SIGNAL(selectCaptureSig()),this,SLOT(selectCaptureSlot()));
    }
}

V4L2Capture::~V4L2Capture()
{
    if(isStreamOn)
    {
        ioctlSetStreamSwitch(false);
    }
    clearSelectResource();
    unMmapBuffers();
    closeDevice();
}
/*
 *@brief:   打开视频采集设备
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@param:   filename:设备文件名(绝对路径)，如/dev/video1
 *@return:  true:成功  false:失败
 */
bool V4L2Capture::openDevice(const char *filename)
{
    cameraFd = open(filename,O_RDWR);//默认阻塞模式打开
    if(cameraFd == -1)
    {
        printf("Cann't open video device(%s).errno=%s\n",filename,strerror(errno));
        return false;
    }
    return true;
}
/*
 *@brief:   关闭视频采集设备
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::closeDevice()
{
    if(cameraFd != -1)
    {
        if(close(cameraFd) == -1)
        {
            printf("Close camera device failed.\n");
        }
        cameraFd = -1;
    }
}
/*
 *@brief:   查询设备的基本信息及驱动能力(v4l2_capability)
 * 通常对于一个摄像设备，它的驱动能力一般仅支持视频采集(V4L2_CAP_VIDEO_CAPTURE)
 * 和ioctl控制(V4L2_CAP_STREAMING)，有些还支持通过系统调用进行读写(V4L2_CAP_READWRITE）
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlQueryCapability()
{
    /*struct v4l2_capability{
        u8 driver[16]; //驱动名字
        u8 card[32]; //设备名字
        u8 bus_info[32]; //设备在系统中的位置
        u32 version; //驱动版本号
        //设备支持的操作，由V4L2_CAP_*宏(在videodev2.h中)对应表示
        u32 capabilities;
        u32 reserved[4]; //保留字段
    };*/
    v4l2_capability capa;
    ioctl(cameraFd,VIDIOC_QUERYCAP,&capa);
    printf("\nBasic Information:\n"
           "driver=%s\tcard=%s\t"
           "bus_info=%s\tversion=%d\tcapabilities=%x\n",
           capa.driver,capa.card,capa.bus_info,capa.version,capa.capabilities);
}
/*
 *@brief:   查询设备支持的模拟视频标准(v4l2_std_id)，比如PAL/NTSC等
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlQueryStd()
{
    v4l2_std_id std;//由V4L2_STD_*宏表示
    ioctl(cameraFd, VIDIOC_QUERYSTD, &std);
    printf("\nAnalog  video standard:%llx\n",std);
}
/*
 *@brief:   查询设备支持的输入(v4l2_input)
 *@author:  缪庆瑞
 *@date:    2020.08.15
 */
void V4L2Capture::ioctlEnumInput()
{
    /*struct v4l2_input {
        __u32	     index;		//要查询的输入序号，从0开始，应用程序设置
        __u8	     name[32];	//输入名称
        __u32	     type;		//输入类型(摄像机或收音机)
        __u32	     audioset;  //音频相关
        __u32        tuner;     //收音机类型
        v4l2_std_id  std;       //模拟信号标准
        __u32	     status;
        __u32	     capabilities;
        __u32	     reserved[3];
    };*/
    struct v4l2_input input;
    input.index = 0;
    printf("\nSupport Input:\n");
    while(ioctl(cameraFd,VIDIOC_ENUMINPUT,&input) != -1)
    {
        printf("input type=%d\tinput name=%s\tinput std=%llx\n",
               input.type,input.name,input.std);
        input.index++;
    }
}
/*
 *@brief:   查询设备支持的帧格式(v4l2_fmtdesc)以及对应格式的分辨率(v4l2_frmsizeenum)
 * 这里查的是采集帧支持的帧格式和分辨率。
 *@author:  缪庆瑞
 *@date:    2020.08.15
 */
void V4L2Capture::ioctlEnumFmt()
{
    /*struct v4l2_fmtdesc{
        u32 index; //要查询的格式序号，应用程序设置
        u32 v4l2_buf_type type; // 帧类型(enum v4l2_buf_type)，应用程序设置
        u32 flags; // 是否为压缩格式
        u8 description[32]; // 格式名称
        u32 pixelformat; //fourcc格式
        u32 reserved[4]; // 保留
    }*/
    v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示 采集帧
    printf("\nSupport Format:\n");
    /*显示所有支持的视频采集帧格式*/
    while(ioctl(cameraFd,VIDIOC_ENUM_FMT,&fmtdesc) != -1)
    {
        printf("flags=%d\tdescription=%s\t"
               "pixelformat=%c%c%c%c\n",
               fmtdesc.flags,fmtdesc.description,
               fmtdesc.pixelformat&0xFF,(fmtdesc.pixelformat>>8)&0xFF,
               (fmtdesc.pixelformat>>16)&0xFF,(fmtdesc.pixelformat>>24)&0xFF);
        /*显示对应采集帧格式所支持的分辨率*/
        /*struct v4l2_frmsizeenum {
            u32 index;	//要查询的帧分辨率序号
            u32 pixel_format;	//fourcc格式
            u32 type;		//设备支持的帧分辨率类型(1分离 2连续 3逐步  0默认分离)
            union {	//帧分辨率
                struct v4l2_frmsize_discrete	discrete;//分离的
                struct v4l2_frmsize_stepwise	stepwise;//逐步的
            };
            u32  reserved[2];	//保留
        };*/
        v4l2_frmsizeenum frmsize;
        frmsize.pixel_format = fmtdesc.pixelformat;
        frmsize.index = 0;
        while(ioctl(cameraFd,VIDIOC_ENUM_FRAMESIZES,&frmsize) != -1)
        {
            printf("\tframesize type=%d\tframesize=%dx%d\n",
                   frmsize.type,frmsize.discrete.width,
                   frmsize.discrete.height);
            frmsize.index++;
        }
        fmtdesc.index++;
    }
}
/*
 *@brief:  获取视频流参数(v4l2_streamparm)，主要是视频采集流支持的模式以及当前模式和帧率
 *@author: 缪庆瑞
 *@date:   2022.08.13
 */
void V4L2Capture::ioctlGetStreamParm()
{
    /*struct v4l2_streamparm {
        __u32	 type;  //帧类型(enum v4l2_buf_type)，应用程序设置
        union {
            struct v4l2_captureparm	capture;//采集参数
            struct v4l2_outputparm	output;//输出参数
            __u8	raw_data[200];  //用户定义
        } parm;
    };*/
    /*struct v4l2_captureparm {//采集参数
        __u32		   capability;	  //支持的模式(0x1表示支持高质量图片采集，0x1000表示支持帧率)
        __u32		   capturemode;	  //当前模式(不支持上面的两种模式，则为0)
        struct v4l2_fract  timeperframe;  //采集帧率
        __u32		   extendedmode;      //驱动特定的扩展模式
        __u32          readbuffers;       //of buffers for read
        __u32		   reserved[4];
    };*/
    v4l2_streamparm streamparm;
    memset(&streamparm,0,sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示采集流
    ioctl(cameraFd,VIDIOC_G_PARM,&streamparm);
    printf("\nCapture Streamparm:\n"
           "Capture capability:%d\tCapture mode:%d\tFrame Rate:%d/%d\n",
           streamparm.parm.capture.capability,
           streamparm.parm.capture.capturemode,
           streamparm.parm.capture.timeperframe.numerator,
           streamparm.parm.capture.timeperframe.denominator);
}
/*
 *@brief:   设置视频流参数(v4l2_streamparm),这里主要是设置视频输入(采集)流的采集模式和帧率
 * 注:该操作在一些平台调用时相对比较费时,但如果不设置的话,可能无法采集正常的图像，比如我们使用的ov9650模块
 * 初始化时不调用该方法，采集的图像就会花屏
 *@author:  缪庆瑞
 *@date:    2022.8.13
 *@param:   highQuality:是否设置高质量图片模式(前提是要设备支持，不支持则设置无效)
 *@param:   timeperframe_n:帧率(即每秒的帧数，前提是支持采集帧率模式，不支持则设置无效)
 */
void V4L2Capture::ioctlSetStreamParm(bool highQuality, uint timeperframe)
{
    v4l2_streamparm streamparm;
    memset(&streamparm,0,sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示采集流
    /*表示采集模式，对于我们使用的ov9650模块，640*480分辨率使用默认0，320×240分辨率设为高质量*/
    streamparm.parm.capture.capturemode = V4L2_CAP_TIMEPERFRAME | (uchar)highQuality;
    /*设置采集流的帧率 1/30即每秒采集30帧，这里设置的值仅是一个期望值(实际值与硬件的属性参数(分辨率)有
     * 关系)，硬件会根据期望值返回最接近的实际支持的设置值*/
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = timeperframe;
    ioctl(cameraFd, VIDIOC_S_PARM, &streamparm);
}
/*
 *@brief:  获取视频流格式(v4l2_format)，这里主要是视频采集流的帧格式(v4l2_pix_format)
 *@author: 缪庆瑞
 *@date:   2022.08.13
 */
void V4L2Capture::ioctlGetStreamFmt()
{
    /*struct v4l2_format {
        __u32	 type;//帧类型(enum v4l2_buf_type)，应用程序设置,该类型决定了下面的union使用哪一个
        union {
            struct v4l2_pix_format		pix;     //V4L2_BUF_TYPE_VIDEO_CAPTURE
            struct v4l2_pix_format_mplane	pix_mp;  //V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
            struct v4l2_window		win;     //V4L2_BUF_TYPE_VIDEO_OVERLAY
            struct v4l2_vbi_format		vbi;     //V4L2_BUF_TYPE_VBI_CAPTURE
            struct v4l2_sliced_vbi_format	sliced;  //V4L2_BUF_TYPE_SLICED_VBI_CAPTURE
            struct v4l2_sdr_format		sdr;     //V4L2_BUF_TYPE_SDR_CAPTURE
            __u8	raw_data[200];               //user-defined
        } fmt;
    };*/
    /*struct v4l2_pix_format {//视频采集帧格式
        __u32           width;//宽
        __u32			height;//高
        __u32			pixelformat;//帧格式
        __u32			field;		//场格式(enum v4l2_field)
        __u32           bytesperline; //一行像素的字节数 for padding, zero if unused
        __u32          	sizeimage;//图像大小
        __u32			colorspace;	//颜色空间(enum v4l2_colorspace)
        ......
    };*/
    v4l2_format format;
    memset(&format,0,sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示采集流
    ioctl(cameraFd,VIDIOC_G_FMT,&format);
    printf("\nCurrent pixformat:\n"
           "pix size:%dx%d\t pixelformat:%c%c%c%c\n"
           "field:%d\t bytesperline:%d\t sizeimage:%d\t colorspace:%d\n",
           format.fmt.pix.width,format.fmt.pix.height,
           format.fmt.pix.pixelformat&0xFF,(format.fmt.pix.pixelformat>>8)&0xFF,
           (format.fmt.pix.pixelformat>>16)&0xFF,(format.fmt.pix.pixelformat>>24)&0xFF,
           format.fmt.pix.field,format.fmt.pix.bytesperline,format.fmt.pix.sizeimage,format.fmt.pix.colorspace);
}
/*
 *@brief:   设置视频流格式(v4l2_streamparm)，这里主要是视频输入(采集)流的格式
 *@author:  缪庆瑞
 *@date:    2022.8.13
 *@param:   pixelformat:帧格式(V4L2_PIX_FMT*)
 *@param:   width:帧宽度  必须是16的倍数
 *@param:   height:帧高度  必须是16的倍数
 */
void V4L2Capture::ioctlSetStreamFmt(uint pixelformat, uint width, uint height)
{
    v4l2_format format;
    memset(&format,0,sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示采集流
    /*设置采集帧格式*/
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = pixelformat;
    format.fmt.pix.field = V4L2_FIELD_ANY;//帧域
    ioctl(cameraFd,VIDIOC_S_FMT,&format);

    //记录当前设置
    pixelFormat = pixelformat;
    pixelWidth = width;
    pixelHeight = height;
}
/*
 *@brief:   申请视频帧缓冲区(v4l2_requestbuffers)，缓冲区在内核空间
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlRequestBuffers()
{
    /*struct v4l2_requestbuffers{
        u32 count; // 缓冲队列里帧的数目，不宜过多
        enum v4l2_buf_type type; //帧类型
        enum v4l2_memory memory; // 用户程序与设备交换数据的方式
        u32 reserved[2];
    };*/
    v4l2_requestbuffers reqbufs;
    memset(&reqbufs,0,sizeof(reqbufs));
    reqbufs.count = BUFFER_COUNT;//缓冲帧数目
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//采集帧
    reqbufs.memory = V4L2_MEMORY_MMAP;//内存映射方式，省却数据拷贝的时间
    ioctl(cameraFd,VIDIOC_REQBUFS,&reqbufs);//申请视频缓冲区
}
/*
 *@brief:   映射视频帧缓冲区(v4l2_buffer)到用户空间内存,便于用户直接访问处理缓冲区的数据
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@return:  bool:true=映射成功  false=映射失败
 */
bool V4L2Capture::ioctlMmapBuffers()
{
    v4l2_buffer vbuffer;//视频缓冲帧
    for(int i = 0;i<BUFFER_COUNT;i++)
    {
        memset(&vbuffer,0,sizeof(vbuffer));
        vbuffer.index = i;//索引号
        vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//采集帧
        vbuffer.memory = V4L2_MEMORY_MMAP;
        //查询指定index的缓冲帧信息(缓冲帧在内核空间的长度和偏移量地址)
        ioctl(cameraFd,VIDIOC_QUERYBUF,&vbuffer);
        /*记录缓存帧的长度及内存映射的地址*/
        bufferMmapPtr[i].length = vbuffer.length;
        bufferMmapPtr[i].addr = (unsigned char *)mmap(NULL,vbuffer.length,PROT_READ,MAP_SHARED,cameraFd,vbuffer.m.offset);
        if(bufferMmapPtr[i].addr == MAP_FAILED)
        {
            printf("mmap failed.\n");
            return false;
        }
    }
    return true;
}
/*
 *@brief:   放缓冲帧进输入队列,驱动将采集到的一帧数据存入该队列的缓冲区，存完后会自动将该帧缓冲区移至采集输出队列。
 *注：该操作在启动采集之前会被自动调用(封装到了ioctlSetStreamSwitch函数内)，确保采集时输入队列正确,
 *所以就无需在外部手动调用了。
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlQueueBuffers()
{
    v4l2_buffer vbuffer;
    for(int i = 0;i < BUFFER_COUNT;i++)
    {
        memset(&vbuffer,0,sizeof(vbuffer));
        vbuffer.index = i;
        vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuffer.memory = V4L2_MEMORY_MMAP;
        ioctl(cameraFd,VIDIOC_QBUF,&vbuffer);//缓冲帧放入视频输入队列 FIFO
    }
}
/*
 *@brief:   从输出队列取缓冲帧，转换成rgb格式的帧
 *注:该函数将内核输出队列的缓冲帧，取出到用户空间并进行格式转换处理，可以认为是软件应用层捕获视频帧显示的最核心处理。
 *需要确保该函数调用处理的时间要快于设备采集帧率，假如帧率是25,那么要保证至少在每40ms以内就得调用该函数处理一次，
 *这也要求该函数内部的处理要尽可能的高效，否则视频帧在界面刷新时就可能显示异常(图像闪烁，旧帧不更新等等)。
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@param:   rgbFrameAddr:rgb格式帧的内存地址,该地址的内存空间必须在方法外申请
 *@return:  bool:true=成功取出一帧
 */
bool V4L2Capture::ioctlDequeueBuffers(uchar *rgbFrameAddr)
{
    v4l2_buffer vbuffer;
    memset(&vbuffer,0,sizeof(vbuffer));
    vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuffer.memory = V4L2_MEMORY_MMAP;
    //从视频输出队列取出一个缓冲帧
    if(ioctl(cameraFd,VIDIOC_DQBUF,&vbuffer) == -1)
    {
        printf("Dequeue buffers failed.\n");
        return false;
    }
    uchar *yuvFrameAddr = bufferMmapPtr[vbuffer.index].addr;//取出的yuv格式帧的地址
    switch (pixelFormat)
    {
    case V4L2_PIX_FMT_YUYV:
        yuyv_to_rgb_shift(yuvFrameAddr,rgbFrameAddr,pixelWidth,pixelHeight);//将yuyv格式帧转换为rgb格式
        break;
    case V4L2_PIX_FMT_NV12:
        nv12_to_rgb_shift(yuvFrameAddr,rgbFrameAddr,pixelWidth,pixelHeight);//将nv12格式帧转换为rgb格式
        break;
    case V4L2_PIX_FMT_NV21:
        nv21_to_rgb_shift(yuvFrameAddr,rgbFrameAddr,pixelWidth,pixelHeight);//将nv21格式帧转换为rgb格式
        break;
    default:
        break;
    }
    //将取出的缓冲帧重新放回输入队列，实现循环采集数据
    ioctl(cameraFd,VIDIOC_QBUF,&vbuffer);

    return true;
}
/*
 *@brief:   启动/停止视频帧采集
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@param:   on:true=启动  false=停止
 */
void V4L2Capture::ioctlSetStreamSwitch(bool on)
{
    v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(on)
    {
        //启动采集数据流，采集之前需要确保输入队列已放满
        ioctlQueueBuffers();
        ioctl(cameraFd,VIDIOC_STREAMON,&type);
    }
    else
    {
        //停止采集数据流，停止后会清空输入队列
        ioctl(cameraFd,VIDIOC_STREAMOFF,&type);
    }
    //标记采集状态
    isStreamOn = on;
}
/*
 *@brief:   释放视频缓冲区的映射内存
 *@author:  缪庆瑞
 *@date:    2022.8.19
 */
void V4L2Capture::unMmapBuffers()
{
    if(bufferMmapPtr[0].addr != NULL)
    {
        for(int i = 0;i<BUFFER_COUNT;i++)
        {
            munmap(bufferMmapPtr[i].addr,bufferMmapPtr[i].length);
            bufferMmapPtr[0].addr = NULL;
        }
    }
}
/*
 *@brief:   使用select机制自动从输出队列取缓冲帧
 *注：该函数内部是一个while循环，为避免阻塞主线程，外部使用信号触发使其工作在子线程，不要直接调用
 *@author:  缪庆瑞
 *@date:    2022.8.16
 */
void V4L2Capture::selectCaptureSlot()
{
    if(!useSelectCapture)
    {
        return;
    }
    //双缓冲(避免通过信号发出去的帧数据来不及处理显示而被下一帧数据覆盖)
    if(selectRgbFrameBuf == NULL)
    {
        selectRgbFrameBuf = (uchar *)malloc(pixelWidth*pixelHeight*3);
    }
    if(selectRgbFrameBuf2 == NULL)
    {
        selectRgbFrameBuf2 = (uchar *)malloc(pixelWidth*pixelHeight*3);
    }
    uchar *curRgbFrameBuf = selectRgbFrameBuf;
    QImage selectRgbFrameImage(selectRgbFrameBuf,pixelWidth,pixelHeight,QImage::Format_RGB888);
    QImage selectRgbFrameImage2(selectRgbFrameBuf2,pixelWidth,pixelHeight,QImage::Format_RGB888);
    QImage curFrameImage = selectRgbFrameImage;

    //select机制所需变量
    fd_set fds,tmp_fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(cameraFd, &fds);
    int ret;
    while(isStreamOn)
    {
        tmp_fds = fds;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        //阻塞直到设备可读或者超时
        ret = select(cameraFd+1, &tmp_fds, NULL, NULL, &tv);
        if(ret == -1)
        {
            if (errno == EINTR)//select会被其他系统调用(比如代码中执行system(""))中断
            {
                continue;
            }
            printf("select error:%d\n",errno);
        }
        else if(ret == 0)
        {
            printf("select timeout.\n");
        }
        else
        {
            //双缓冲交换
            if(curRgbFrameBuf == selectRgbFrameBuf2)
            {
                curRgbFrameBuf = selectRgbFrameBuf;
                curFrameImage = selectRgbFrameImage;
            }
            else
            {
                curRgbFrameBuf = selectRgbFrameBuf2;
                curFrameImage = selectRgbFrameImage2;
            }
            //获取并处理队列里的缓冲帧
            if(ioctlDequeueBuffers(curRgbFrameBuf))
            {
                emit selectCaptureFrameSig(curRgbFrameBuf);
                emit selectCaptureFrameSig(curFrameImage);
            }
        }
    }
}
/*
 *@brief:   清理select机制申请的相关资源
 *@author:  缪庆瑞
 *@date:    2022.8.19
 */
void V4L2Capture::clearSelectResource()
{
    //退出线程
    if(selectThread)
    {
        selectThread->exit();
        if(!selectThread->wait(3000))
        {
            selectThread->terminate();
            selectThread->wait(3000);
        }
    }
    //释放缓冲帧内存
    if(selectRgbFrameBuf)
    {
        free(selectRgbFrameBuf);
    }
    if(selectRgbFrameBuf2)
    {
        free(selectRgbFrameBuf2);
    }
}
/*
 *@brief:   采用整形移位方式转换YUV--RGB，由于YUV大多格式都是多个Y分量共用一组uv，所以为了减少重复计算，
 *关于uv分量的整形移位处理不纳入到该函数中,该函数主要用来关联Y分量，并对rbg值做范围校验。
 *注:该函数需要对每一个像素点进行处理计算，调用频次比较高，故设置成内联函数降低函数调用的损失，但相较与直接
 *编码性能还是会低一些(在1.2GHz的arm上实测，720×576分辨率每一帧处理会多用1-2ms(Debug模式编译的程序时间
 *差距会更大))，如果对效率要求较高建议采取硬编码。
 *@author:  缪庆瑞
 *@date:    2022.8.18
 *@param:   y:Y分量值
 *@param:   r_uv:根据移位法求得的未关联Y的R值
 *@param:   g_uv:根据移位法求得的未关联Y的G值
 *@param:   b_uv:根据移位法求得的未关联Y的B值
 *@param:   rgb:存放一个rgb像素(3字节)的首地址
 */
void V4L2Capture::yuv_to_rgb_shift(int &y, int r_uv, int g_uv, int b_uv, uchar rgb[])
{
    /*移位法  计算RGB公式内不包含Y的部分，结果可以供多个rgb像素使用
    r_uv = v+((103*v)>>8);
    g_uv = ((88*u)>>8)+((183*v)>>8);
    b_uv = u+((197*u)>>8);*/

    //关联Y分量，计算出rgb值
    r_uv = y + r_uv;
    g_uv = y - g_uv;
    b_uv = y + b_uv;
    //校验范围
    rgb[0] = (r_uv > 255)?255:(r_uv < 0)?0:r_uv;
    rgb[1] = (g_uv > 255)?255:(g_uv < 0)?0:g_uv;
    rgb[2] = (b_uv > 255)?255:(b_uv < 0)?0:b_uv;
}
/*
 *@brief:   将yuyv帧格式数据转换成rgb格式数据，这里采用的是基于整形移位的yuv--rgb转换公式
 *注:YUYV是YUV422采样方式中的一种，数据存储方式分为packed(打包)和planar(平面)，该函数是基于
 *packed方式的转换。
 *@author:  缪庆瑞
 *@date:    2019.8.7
 *@param:   yuyv:yuyv帧格式数据地址，该地址通常是对设备的内存映射空间
 *@param:   rgb:rgb帧格式数据地址，该地址内存空间必须在方法外申请
 *@param:   width:宽度
 *@param:   height:高度
 */
void V4L2Capture::yuyv_to_rgb_shift(uchar *yuyv, uchar *rgb, uint width, uint height)
{
    //qDebug()<<"yuyv_to_rgb_shift-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    int yvyuLen = width*height*2;//yuyv用四字节表示两个像素
    int y0,u,y1,v;
    int r_uv,g_uv,b_uv;
    int r,g,b;
    int rgbIndex = 0;
    /*每次循环转换出两个rgb像素*/
    for(int i = 0;i<yvyuLen;i += 4)
    {
        //按顺序提取yuyv数据
        y0 = yuyv[i+0];
        u  = yuyv[i+1] - 128;
        y1 = yuyv[i+2];
        v  = yuyv[i+3] - 128;
        //移位法  计算RGB公式内不包含Y的部分，结果可以供两个rgb像素使用
        r_uv = v+((103*v)>>8);
        g_uv = ((88*u)>>8)+((183*v)>>8);
        b_uv = u+((197*u)>>8);
        //像素1的rgb数据
        r = y0 + r_uv;
        g = y0 - g_uv;
        b = y0 + b_uv;
        rgb[rgbIndex++] = (r > 255)?255:(r < 0)?0:r;
        rgb[rgbIndex++] = (g > 255)?255:(g < 0)?0:g;
        rgb[rgbIndex++] = (b > 255)?255:(b < 0)?0:b;
        //像素2的rgb数据
        r = y1 + r_uv;
        g = y1 - g_uv;
        b = y1 + b_uv;
        rgb[rgbIndex++] = (r > 255)?255:(r < 0)?0:r;
        rgb[rgbIndex++] = (g > 255)?255:(g < 0)?0:g;
        rgb[rgbIndex++] = (b > 255)?255:(b < 0)?0:b;
    }
    //qDebug()<<"yuyv_to_rgb_shift-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
}
/*
 *@brief:   将NV12帧格式数据转换成rgb格式数据，这里采用的是基于整形移位的yuv--rgb转换公式
 *注：NV12是YUV420SP格式的一种，two-plane模式，即Y和UV分为两个plane，Y按照和planar存储，
 *UV(CbCr)则为packed交错存储。
 *@author:  缪庆瑞
 *@date:    2022.8.13
 *@param:   nv12:NV12(YUV420SP的一种)帧格式数据地址，该地址通常是对设备的内存映射空间
 *@param:   rgb:rgb帧格式数据地址，该地址内存空间必须在方法外申请
 *@param:   width:宽度
 *@param:   height:高度
 */
void V4L2Capture::nv12_to_rgb_shift(uchar *nv12, uchar *rgb, uint width, uint height)
{
    //qDebug()<<"nv12_to_rgb_shift-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    uint y_len,rgb_width,cur_pixel_pos,cur_row_pixel_len;
    uint cur_pixel_rgb_pos,next_pixel_rgb_pos;
    int y_odd1,y_odd2,y_even1,y_even2,u,v;
    int r_uv,g_uv,b_uv;
    int r,g,b;
    y_len = width*height;//Y分量的字节长度
    rgb_width = width*3;//一行rgb像素的字节长度
    cur_pixel_pos = 0;//当前处理的像素位置
    for(uint i=0;i<height;i+=2)//一次处理两行
    {
        cur_row_pixel_len = cur_pixel_pos;//当前行首的像素距离首个像素的长度
        for(uint j=0;j<width;j+=2)//一次处理两列
        {
            //当前的rgb像素位置 = 当前处理像素位置*3
            cur_pixel_rgb_pos = cur_pixel_pos*3;
            next_pixel_rgb_pos = cur_pixel_rgb_pos + rgb_width;
            //uv分量
            u = nv12[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos] - 128;
            v = nv12[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos+1] - 128;
            //移位法  计算RGB公式内不包含Y的部分，结果可以供4个rgb像素使用
            r_uv = v+((103*v)>>8);
            g_uv = ((88*u)>>8)+((183*v)>>8);
            b_uv = u+((197*u)>>8);
            //四个Y分量，共用一组uv
            y_odd1 = nv12[cur_pixel_pos];
            y_odd2 = nv12[cur_pixel_pos+1];
            y_even1 = nv12[cur_pixel_pos+width];
            y_even2 = nv12[cur_pixel_pos+1+width];
            /*关联Y分量，计算出rgb值*/
            //奇数行
            r = y_odd1 + r_uv;
            g = y_odd1 - g_uv;
            b = y_odd1 + b_uv;
            rgb[cur_pixel_rgb_pos] = (r > 255)?255:(r < 0)?0:r;
            rgb[cur_pixel_rgb_pos+1] = (g > 255)?255:(g < 0)?0:g;
            rgb[cur_pixel_rgb_pos+2] = (b > 255)?255:(b < 0)?0:b;
            r = y_odd2 + r_uv;
            g = y_odd2 - g_uv;
            b = y_odd2 + b_uv;
            rgb[cur_pixel_rgb_pos+3] = (r > 255)?255:(r < 0)?0:r;
            rgb[cur_pixel_rgb_pos+4] = (g > 255)?255:(g < 0)?0:g;
            rgb[cur_pixel_rgb_pos+5] = (b > 255)?255:(b < 0)?0:b;
            //偶数行
            r = y_even1 + r_uv;
            g = y_even1 - g_uv;
            b = y_even1 + b_uv;
            rgb[next_pixel_rgb_pos] = (r > 255)?255:(r < 0)?0:r;
            rgb[next_pixel_rgb_pos+1] = (g > 255)?255:(g < 0)?0:g;
            rgb[next_pixel_rgb_pos+2] = (b > 255)?255:(b < 0)?0:b;
            r = y_even2 + r_uv;
            g = y_even2 - g_uv;
            b = y_even2 + b_uv;
            rgb[next_pixel_rgb_pos+3] = (r > 255)?255:(r < 0)?0:r;
            rgb[next_pixel_rgb_pos+4] = (g > 255)?255:(g < 0)?0:g;
            rgb[next_pixel_rgb_pos+5] = (b > 255)?255:(b < 0)?0:b;

            //像素位置向后移动两列
            cur_pixel_pos += 2;
        }
        //像素位置向后跳过一行
        cur_pixel_pos += width;
    }
    //qDebug()<<"nv12_to_rgb_shift-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
}
/*
 *@brief:   将NV21帧格式数据转换成rgb格式数据，这里采用的是基于整形移位的yuv--rgb转换公式
 *注：NV21是YUV420SP格式的一种，two-plane模式，即Y和VU分为两个plane，Y按照和planar存储，
 *VU(CrCb)则为packed交错存储。
 *@author:  缪庆瑞
 *@date:    2022.8.15
 *@param:   nv21:NV21(YUV420SP的一种)帧格式数据地址，该地址通常是对设备的内存映射空间
 *@param:   rgb:rgb帧格式数据地址，该地址内存空间必须在方法外申请
 *@param:   width:宽度
 *@param:   height:高度
 */
void V4L2Capture::nv21_to_rgb_shift(uchar *nv21, uchar *rgb, uint width, uint height)
{
    //qDebug()<<"nv21_to_rgb_shift-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    uint y_len,rgb_width,cur_pixel_pos,cur_row_pixel_len;
    uint cur_pixel_rgb_pos,next_pixel_rgb_pos;
    int y_odd1,y_odd2,y_even1,y_even2,u,v;
    int r_uv,g_uv,b_uv;
    y_len = width*height;//Y分量的字节长度
    rgb_width = width*3;//一行rgb像素的字节长度
    cur_pixel_pos = 0;//当前处理的像素位置
    for(uint i=0;i<height;i+=2)//一次处理两行
    {
        cur_row_pixel_len = cur_pixel_pos;//当前行首的像素距离首个像素的长度
        for(uint j=0;j<width;j+=2)//一次处理两列
        {
            //当前的rgb像素位置 = 当前处理像素位置*3
            cur_pixel_rgb_pos = cur_pixel_pos*3;
            next_pixel_rgb_pos = cur_pixel_rgb_pos + rgb_width;
            //uv分量
            v = nv21[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos] - 128;
            u = nv21[y_len-(cur_row_pixel_len>>1)+cur_pixel_pos+1] - 128;
            //移位法  计算RGB公式内不包含Y的部分，结果可以供4个rgb像素使用
            r_uv = v+((103*v)>>8);
            g_uv = ((88*u)>>8)+((183*v)>>8);
            b_uv = u+((197*u)>>8);
            //四个Y分量，共用一组uv
            y_odd1 = nv21[cur_pixel_pos];
            y_odd2 = nv21[cur_pixel_pos+1];
            y_even1 = nv21[cur_pixel_pos+width];
            y_even2 = nv21[cur_pixel_pos+1+width];
            /*关联Y分量，计算出rgb值*/
            //奇数行
            yuv_to_rgb_shift(y_odd1,r_uv,g_uv,b_uv,rgb+cur_pixel_rgb_pos);
            yuv_to_rgb_shift(y_odd2,r_uv,g_uv,b_uv,rgb+cur_pixel_rgb_pos+3);
            //偶数行
            yuv_to_rgb_shift(y_even1,r_uv,g_uv,b_uv,rgb+next_pixel_rgb_pos);
            yuv_to_rgb_shift(y_even2,r_uv,g_uv,b_uv,rgb+next_pixel_rgb_pos+3);

            //像素位置向后移动两列
            cur_pixel_pos += 2;
        }
        //像素位置向后跳过一行
        cur_pixel_pos += width;
    }
    //qDebug()<<"nv21_to_rgb_shift-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
}
