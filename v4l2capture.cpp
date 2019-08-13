/*
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@brief:   该模块使用V4L2的数据结构和接口,采集视频帧,并转换为rgb格式
 */
#include "v4l2capture.h"
#include <QTime>
#include <QDebug>

/* 用于查表法进行yuv--rgb转换，UV取值范围[0,255]
 * table_rv[i] = 1.403*(V−128)      i==V
 * table_gu[i] = 0.343*(U–128)      i==U
 * table_gv[i] = 0.714*(V–128)      i==V
 * table_bu[i] = 1.770*(U–128)      i==U
 */
int V4L2Capture::table_rv[256] = {-180,-178,-177,-175,-174,-173,-171,-170,-168,-167,-166,-164,-163,-161,-160,-159,-157,-156,-154,-153,-152,-150,-149,-147,-146,-145,-143,-142,-140,-139,-137,-136,-135,-133,-132,-130,-129,-128,-126,-125,-123,-122,-121,-119,-118,-116,-115,-114,-112,-111,-109,-108,-107,-105,-104,-102,-101,-100,-98,-97,-95,-94,-93,-91,-90,-88,-87,-86,-84,-83,-81,-80,-79,-77,-76,-74,-73,-72,-70,-69,-67,-66,-65,-63,-62,-60,-59,-58,-56,-55,-53,-52,-51,-49,-48,-46,-45,-43,-42,-41,-39,-38,-36,-35,-34,-32,-31,-29,-28,-27,-25,-24,-22,-21,-20,-18,-17,-15,-14,-13,-11,-10,-8,-7,-6,-4,-3,-1,0,1,3,4,6,7,8,10,11,13,14,15,17,18,20,21,22,24,25,27,28,29,31,32,34,35,36,38,39,41,42,43,45,46,48,49,51,52,53,55,56,58,59,60,62,63,65,66,67,69,70,72,73,74,76,77,79,80,81,83,84,86,87,88,90,91,93,94,95,97,98,100,101,102,104,105,107,108,109,111,112,114,115,116,118,119,121,122,123,125,126,128,129,130,132,133,135,136,137,139,140,142,143,145,146,147,149,150,152,153,154,156,157,159,160,161,163,164,166,167,168,170,171,173,174,175,177,178};
int V4L2Capture::table_gu[256] = {-44,-44,-43,-43,-43,-42,-42,-42,-41,-41,-40,-40,-40,-39,-39,-39,-38,-38,-38,-37,-37,-37,-36,-36,-36,-35,-35,-35,-34,-34,-34,-33,-33,-33,-32,-32,-32,-31,-31,-31,-30,-30,-29,-29,-29,-28,-28,-28,-27,-27,-27,-26,-26,-26,-25,-25,-25,-24,-24,-24,-23,-23,-23,-22,-22,-22,-21,-21,-21,-20,-20,-20,-19,-19,-19,-18,-18,-17,-17,-17,-16,-16,-16,-15,-15,-15,-14,-14,-14,-13,-13,-13,-12,-12,-12,-11,-11,-11,-10,-10,-10,-9,-9,-9,-8,-8,-8,-7,-7,-7,-6,-6,-5,-5,-5,-4,-4,-4,-3,-3,-3,-2,-2,-2,-1,-1,-1,0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,7,7,7,8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15,16,16,16,17,17,17,18,18,19,19,19,20,20,20,21,21,21,22,22,22,23,23,23,24,24,24,25,25,25,26,26,26,27,27,27,28,28,28,29,29,29,30,30,31,31,31,32,32,32,33,33,33,34,34,34,35,35,35,36,36,36,37,37,37,38,38,38,39,39,39,40,40,40,41,41,42,42,42,43,43,43,44};
int V4L2Capture::table_gv[256] = {-91,-91,-90,-89,-89,-88,-87,-86,-86,-85,-84,-84,-83,-82,-81,-81,-80,-79,-79,-78,-77,-76,-76,-75,-74,-74,-73,-72,-71,-71,-70,-69,-69,-68,-67,-66,-66,-65,-64,-64,-63,-62,-61,-61,-60,-59,-59,-58,-57,-56,-56,-55,-54,-54,-53,-52,-51,-51,-50,-49,-49,-48,-47,-46,-46,-45,-44,-44,-43,-42,-41,-41,-40,-39,-39,-38,-37,-36,-36,-35,-34,-34,-33,-32,-31,-31,-30,-29,-29,-28,-27,-26,-26,-25,-24,-24,-23,-22,-21,-21,-20,-19,-19,-18,-17,-16,-16,-15,-14,-14,-13,-12,-11,-11,-10,-9,-9,-8,-7,-6,-6,-5,-4,-4,-3,-2,-1,-1,0,1,1,2,3,4,4,5,6,6,7,8,9,9,10,11,11,12,13,14,14,15,16,16,17,18,19,19,20,21,21,22,23,24,24,25,26,26,27,28,29,29,30,31,31,32,33,34,34,35,36,36,37,38,39,39,40,41,41,42,43,44,44,45,46,46,47,48,49,49,50,51,51,52,53,54,54,55,56,56,57,58,59,59,60,61,61,62,63,64,64,65,66,66,67,68,69,69,70,71,71,72,73,74,74,75,76,76,77,78,79,79,80,81,81,82,83,84,84,85,86,86,87,88,89,89,90,91};
int V4L2Capture::table_bu[256] = {-227,-225,-223,-221,-219,-218,-216,-214,-212,-211,-209,-207,-205,-204,-202,-200,-198,-196,-195,-193,-191,-189,-188,-186,-184,-182,-181,-179,-177,-175,-173,-172,-170,-168,-166,-165,-163,-161,-159,-158,-156,-154,-152,-150,-149,-147,-145,-143,-142,-140,-138,-136,-135,-133,-131,-129,-127,-126,-124,-122,-120,-119,-117,-115,-113,-112,-110,-108,-106,-104,-103,-101,-99,-97,-96,-94,-92,-90,-88,-87,-85,-83,-81,-80,-78,-76,-74,-73,-71,-69,-67,-65,-64,-62,-60,-58,-57,-55,-53,-51,-50,-48,-46,-44,-42,-41,-39,-37,-35,-34,-32,-30,-28,-27,-25,-23,-21,-19,-18,-16,-14,-12,-11,-9,-7,-5,-4,-2,0,2,4,5,7,9,11,12,14,16,18,19,21,23,25,27,28,30,32,34,35,37,39,41,42,44,46,48,50,51,53,55,57,58,60,62,64,65,67,69,71,73,74,76,78,80,81,83,85,87,89,90,92,94,96,97,99,101,103,104,106,108,110,112,113,115,117,119,120,122,124,126,127,129,131,133,135,136,138,140,142,143,145,147,149,150,152,154,156,158,159,161,163,165,166,168,170,172,173,175,177,179,181,182,184,186,188,189,191,193,195,196,198,200,202,204,205,207,209,211,212,214,216,218,219,221,223,225};

V4L2Capture::V4L2Capture(QObject *parent):QObject(parent)
{
    cameraFd = -1;
}

V4L2Capture::~V4L2Capture()
{
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
        printf("Cann't open video device(%s).\n",filename);
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
    if(close(cameraFd) == -1)
    {
        printf("Close camera device failed.\n");
    }
}
/*
 *@brief:   初始化采集设备，用于在视频帧采集前初始化
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
bool V4L2Capture::initDevice()
{
    qDebug()<<"initDevice-start:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    if(!openDevice(VIDEO_DEVICE))
    {
        return false;
    }
    /*检查设备的一些基本信息(可选项)*/
    ioctlQueryCapability();
    ioctlQueryStd();
    ioctlEnumFmt();
    /*设置采集数据的一些参数及格式(必选项)*/
    ioctlSetStreamParm();
    ioctlSetStreamFmt();
    /*申请视频帧缓冲区，并进行相关处理(必选项)*/
    ioctlRequestBuffers();
    ioctlMmapBuffers();
    ioctlQueueBuffers();
    qDebug()<<"initDevice-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
    return true;
}
/*
 *@brief:   查询设备的基本信息及驱动能力(v4l2_capability)
 * 通常对于一个摄像设备，它的驱动能力一般仅支持视频采集(V4L2_CAP_VIDEO_CAPTURE)
 * 和ioctl控制(V4L2_CAP_STREAMING)
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
 *@brief:   显示设备支持的帧格式(v4l2_fmtdesc)，这里查的是采集帧支持的格式
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlEnumFmt()
{
    /*struct v4l2_fmtdesc{
        u32 index; //要查询的格式序号，应用程序设置
        enum v4l2_buf_type type; // 帧类型，应用程序设置
        u32 flags; // 是否为压缩格式
        u8 description[32]; // 格式名称
        u32 pixelformat; //fourcc格式
        u32 reserved[4]; // 保留
    }*/
    v4l2_fmtdesc fmtdesc;
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示 采集帧
    printf("\nSupport Format:\n");
    //显示所有支持的视频采集帧格式
    while(ioctl(cameraFd,VIDIOC_ENUM_FMT,&fmtdesc) != -1)
    {
        printf("flags=%d\tdescription=%s\t"
               "pixelformat=%c%c%c%c\n",
               fmtdesc.flags,fmtdesc.description,
               fmtdesc.pixelformat&0xFF,(fmtdesc.pixelformat>>8)&0xFF,
               (fmtdesc.pixelformat>>16)&0xFF,(fmtdesc.pixelformat>>24)&0xFF);
        fmtdesc.index++;
    }
}
/*
 *@brief:   设置视频流参数(v4l2_streamparm),这里主要是设置视频输入(采集)流的采集模式和帧率
 * 注:该操作相对比较费时,但如果不设置的话,可能无法采集正常的图像，比如我们使用的ov9650模块
 * 初始化时不调用该方法，采集的图像就会花屏
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlSetStreamParm()
{
    v4l2_streamparm streamparm;
    memset(&streamparm,0,sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示采集流
    /*表示采集模式，采集高质量图片值设为1，一般设为0。
     * 对于我们使用的ov9650模块，640*480分辨率设为0，320×240分辨率设为1*/
    streamparm.parm.capture.capturemode = 0;
    /*设置采集流的帧率 1/30即每秒采集30帧，这里设置的值仅是一个期望值(实际值与硬件
     * 的属性参数(分辨率)有关系)，硬件会根据期望值返回最接近的实际支持的设置值*/
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = 30;
    ioctl(cameraFd, VIDIOC_S_PARM, &streamparm);
    //获取设置的帧率
    ioctl(cameraFd, VIDIOC_G_PARM, &streamparm);
    printf("\nCapturemode:%d\nFrame Rate:%d/%d\n",
           streamparm.parm.capture.capturemode,
           streamparm.parm.capture.timeperframe.numerator,
           streamparm.parm.capture.timeperframe.denominator);
}
/*
 *@brief:   设置视频流格式(v4l2_streamparm)，这里主要是视频输入(采集)流的格式
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::ioctlSetStreamFmt()
{
    /*struct v4l2_pix_format{//采集帧格式
        u32 width; // 帧宽，单位像素
        u32 height; // 帧高，单位像素
        u32 pixelformat; // 帧格式
        enum v4l2_field field;//帧域
        ......
    };*/
    v4l2_format format;
    memset(&format,0,sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//表示采集流
    /*设置采集帧格式*/
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//帧格式，通常的视频采集设备都支持yuyv格式
    format.fmt.pix.width = FRAME_WIDTH;//帧宽度 必须是16的倍数
    format.fmt.pix.height = FRAME_HEIGHT;//帧高度 必须是16的倍数
    format.fmt.pix.field = V4L2_FIELD_ANY;//帧域
    ioctl(cameraFd,VIDIOC_S_FMT,&format);
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
 */
void V4L2Capture::ioctlMmapBuffers()
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
        bufferMmapPtr[i].addr = (unsigned char *)mmap(0,vbuffer.length,PROT_READ,MAP_SHARED,cameraFd,vbuffer.m.offset);
        if(bufferMmapPtr[i].addr == MAP_FAILED)
        {
            printf("mmap failed.\n");
            exit(-1);
        }
    }
}
/*
 *@brief:   放缓冲帧进输入队列。驱动将采集到的一帧视频数据存入该队列的一帧缓冲区，
 * 存完后会自动将该帧缓冲区移至视频采集输出队列。
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
 *@author:  缪庆瑞
 *@date:    2019.08.07
 *@param:   rgbFrameAddr:rgb格式帧的内存地址,该地址的内存空间必须在方法外申请
 */
void V4L2Capture::ioctlDequeueBuffers(uchar *rgbFrameAddr)
{
    v4l2_buffer vbuffer;
    memset(&vbuffer,0,sizeof(vbuffer));
    vbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuffer.memory = V4L2_MEMORY_MMAP;
    //从视频输出队列取出一个缓冲帧
    if(ioctl(cameraFd,VIDIOC_DQBUF,&vbuffer) == -1)
    {
        printf("Dequeue buffers failed.\n");
        return;
    }
    uchar *yuyvFrameAddr = bufferMmapPtr[vbuffer.index].addr;//取出的yuyv格式帧的地址
    yuyv_to_rgb_shift(yuyvFrameAddr,rgbFrameAddr,FRAME_WIDTH,FRAME_HEIGHT);//将yuyv格式帧转换为rgb格式
    //将取出的缓冲帧重新放回输入队列，实现循环采集数据
    ioctl(cameraFd,VIDIOC_QBUF,&vbuffer);
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
        /*启动采集数据流，放入视频输入队列缓冲帧,填满一帧移到视频输出缓冲队列*/
        ioctl(cameraFd,VIDIOC_STREAMON,&type);
    }
    else
    {
        ioctl(cameraFd,VIDIOC_STREAMOFF,&type);//停止采集数据流
    }
}
/*
 *@brief:   释放视频缓冲区的映射内存
 *@author:  缪庆瑞
 *@date:    2019.08.07
 */
void V4L2Capture::unMmapBuffers()
{
    for(int i = 0;i<BUFFER_COUNT;i++)
    {
        munmap(bufferMmapPtr[i].addr,bufferMmapPtr[i].length);
    }
}
/*
 *@brief:   将yuyv帧格式数据转换成rgb格式数据，这里采用的是基于整形移位的yuv--rgb转换公式:
 * v=V-128; u=U-128;
 * R=Y+v+((103*v)>>8)
 * G=Y-((88*u)>>8)-((183*v)>>8)
 * B=Y+u+((197*u)>>8)
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
    int r,g,b;
    int rgbIndex = 0;
    int r_uv,g_uv,b_uv;
    /*每次循环转换出两个rgb像素*/
    for(int i = 0;i<yvyuLen;i += 4)
    {
        //按顺序提取yuyv数据
        y0 = yuyv[i+0];
        u  = yuyv[i+1] - 128;
        y1 = yuyv[i+2];
        v  = yuyv[i+3] - 128;
        /*计算RGB公式内不包含Y的部分，结果可以供两个rgb像素使用*/
        /*//查表法(使用该方法注意上面提取uv数据时不要减128)  和移位法效率差不多
        r_uv = table_rv[v];
        g_uv = table_gu[u]+table_gv[v];
        b_uv = table_bu[u];*/
        //移位法
        r_uv = v+((103*v)>>8);
        g_uv = ((88*u)>>8)+((183*v)>>8);
        b_uv = u+((197*u)>>8);
        /*根据公式得到像素1的rgb数据*/
        r = y0 + r_uv;
        g = y0 - g_uv;
        b = y0 + b_uv;
        //限制rgb的范围
        r = (r > 255)?255:(r < 0)?0:r;
        g = (g > 255)?255:(g < 0)?0:g;
        b = (b > 255)?255:(b < 0)?0:b;
        rgb[rgbIndex++] = (uchar)r;
        rgb[rgbIndex++] = (uchar)g;
        rgb[rgbIndex++] = (uchar)b;
        /*根据公式得到像素2的rgb数据*/
        r = y1 + r_uv;
        g = y1 - g_uv;
        b = y1 + b_uv;
        r = (r > 255)?255:(r < 0)?0:r;
        g = (g > 255)?255:(g < 0)?0:g;
        b = (b > 255)?255:(b < 0)?0:b;
        rgb[rgbIndex++] = (uchar)r;
        rgb[rgbIndex++] = (uchar)g;
        rgb[rgbIndex++] = (uchar)b;
    }
    //qDebug()<<"yuyv_to_rgb_shift-end:"<<QTime::currentTime().toString("hh:mm:ss:zzz");
}
