# V4L2VideoProcess
这是一个使用V4L2(Video for Linux two)的数据结构和接口，旨在完成Linux系统下视频数据处理的各模块功能封装的Qt项目。(目前实现了视频采集模块的功能封装)

## 概述
V4L2是Linux环境下开发视频采集设备驱动程序的一套规范(API)，可用于图片、视频和音频数据的采集。它为驱动程序的编写以及应用程序的访问提供统一的接口，并将所有的视频采集设备的驱动程序都纳入其的管理之中。不仅给驱动程序编写者带来极大的方便，同时也方便了应用程序的编写和移植。在远程会议、可视电话、视频监控系统和嵌入式多媒体终端中都有广泛的应用。（V4L2功能可在Linux内核编译阶段配置，默认情况下都有此开发接口）。  
V4L2规范中不仅定义了通用API元素，图像的格式，输入/输出方法，还定义了Linux内核驱动处理视频信息的一系列接口(Interfaces)，这些接口主要有：  
```
视频采集接口——Video Capture Interface;
视频输出接口—— Video Output Interface;
视频覆盖/预览接口——Video Overlay Interface;
视频输出覆盖接口——Video Output Overlay Interface;
编解码接口——Codec Interface。
```
## 功能模块
### 1.视频采集模块
#### 1.1.采集流程
1. 打开视频设备文件，检查设备信息，设置采集数据的参数和格式，比如采集帧的帧率、帧格式、采集模式等;
2. 申请若干视频采集的帧缓冲区，并将这些帧缓冲区从内核空间映射到用户空间，便于应用程序读取/处理视频数据;
3. 将申请到的帧缓冲区在视频采集输入队列排队，并启动视频采集;
4. 驱动开始视频数据的采集，应用程序从视频采集输出队列取出帧缓冲区，处理完后，将帧缓冲区重新放入视频采集输入队列，循环往复采集连续的视频数据;
5. 停止视频采集。

#### 1.2.帧格式转换(YUV--RGB)
在计算机视觉系统中，图像的显示往往涉及到颜色空间转换的问题，目前广泛采用的数字图像传感器和模拟图像传感器的数据输出格式一般都包含YUV(YCbCr)，而显示器驱动以及图像处理往往都是采用RGB值，所以就需要YUV--RGB帧格式的转换。  
##### 1.2.1.YUV详解
YUV，分为三个分量，“Y”表示明亮度，也就是灰度值；而“U”和“V” 表示的则是色度，作用是描述影像色彩及饱和度，用于指定像素的颜色。YCbCr是YUV通过缩放和偏移衍变而成的，Cb、Cr同样指色彩(红蓝色差分量)，只是表示方法不同，一般情况下YUV和YCbCr被认为是相同的。  
与RGB类似，YUV也是一种颜色编码方法。它的存储格式有两大类：平面planar(先连续存储所有像素点的Y，紧接着存储所有像素点的U，随后是所有像素点的V)格式和打包packed(每个像素点的Y,U,V是连续交叉存储的)格式。而在这两大类之下根据采样方式(Y U V采样频率比例)又分为YUV444、YUV422、YUV420等等。  
```
YUV 4:4:4采样，该格式亮度和色度的采样比例相同，每一个Y对应一组UV分量，但同时传输的数据量也最大，所以在视频采集时不太常用。
YUV 4:2:2采样，该格式亮度和色度采样比例2:1，每两个Y共用一组UV分量，常见的YUYV(YUY2)、UYVY，YUV422P(planar)都属于该格式。
YUV 4:2:0采样，该格式亮度和色度采样比例4:1，每四个Y共用一组UV分量，常见的NV12、NV21都属于该格式。
```
注:有关更多的YUV格式及采样情况，请自行上网搜索，另外在document/v4l2.pdf(v4l2 API说明书)文档内也提供了一些常见yuv格式采样的字节顺序。
##### 1.2.2.YUV--RGB转换公式
网上关于YUV--RGB的转换公式有好多种，而且有些差别很大，但那些公式基本都是对的，只不过是公式的定义域与值域不同。如果将公式中的 YUV 和 RGB 的取值范围统一成相同的，那么最终得到的公式基本一致，即便系数有些细微的差别，但最后计算的结果差异很小，基本上眼睛看不出区别来。
```
yuv<---->rgb格式转换常用公式(CCIR 601)：
浮点计算(效率低) 
R=Y+1.403*(V−128)
G=Y–0.343*(U–128)–0.714*(V–128)
B=Y+1.770*(U–128)

Y=0.299R+0.587G+0.114B
U(Cb)=−0.169R−0.331G+0.500B+128
V(Cr)=0.500R−0.419G−0.081B+128
整形移位:(效率较高)
v=V-128; u=U-128;
R=Y+v+((103*v)>>8)
G=Y-((88*u)>>8)-((183*v)>>8)
B=Y+u+((197*u)>>8)
```
### 1.3.代码功能及接口  
#### 1.3.1.模块功能
1.该模块使用V4L2的标准流程和接口采集视频帧，使用mmap内存映射的方式实现从内核空间取帧数据到用户空间。  
2.该模块提供两种取帧方式：一种是在类外定时调用指定接口(ioctlDequeueBuffers)取帧，可以自由控制软件的取帧频次，不过有些设备驱动当取帧频次小于硬件帧率时，显示会异常;另一种是采用select机制自动取帧，该方式在处理性能跟得上的情况下，取帧速率跟帧率一致，每取完一帧数据以信号的形式对外发送。要使用该方式只需在类构造函数中传递useSelect=true参数，内部会自动创建子线程自动完成取帧处理，外部只需绑定相关信号即可。  
3.该模块目前集成了V4L2_PIX_FMT_YUYV、V4L2_PIX_FMT_NV12、V4L2_PIX_FMT_NV21三种yuv格式到rgb的转换处理，均是使用CCIR 601的转换公式(整形移位)，为了提高处理性能，转换函数已经尽最大可能的进行了优化。  
4.采集模块的代码由V4L2Capture类封装，VideoDisplayWidget类作为一个demo实现视频预览，为了降低视频帧刷新对cpu的占用，使用一个继承自QOpenGLWidget的PixmapWidget类显示图像，便于硬件加速。如果平台不支持opengl，则可以换用QLabel显示。
#### 1.3.2.代码接口  
```
bool openDevice(const char *filename);//打开设备
void closeDevice();//关闭设备

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
void ioctlMmapBuffers();//映射视频帧缓冲区到用户空间内存
//帧采集控制
void ioctlQueueBuffers();//放缓冲帧进输入队列
bool ioctlDequeueBuffers(uchar *rgbFrameAddr);//从输出队列取缓冲帧
void ioctlSetStreamSwitch(bool on);//启动/停止视频帧采集

void unMmapBuffers();//释放视频缓冲区的映射内存
//yuv--rgb转换
inline void yuv_to_rgb_shift(int &y,int r_uv,int g_uv,int b_uv,uchar rgb[]);
void yuyv_to_rgb_shift(uchar *yuyv,uchar *rgb,uint width,uint height);
void nv12_to_rgb_shift(uchar *nv12,uchar *rgb,uint width,uint height);
void nv21_to_rgb_shift(uchar *nv21,uchar *rgb,uint width,uint height);

signals:
    //向外发射信号(rgb原始数据流和封装的QImage，按需使用)
    void selectCaptureFrameSig(uchar *rgbFrame);
    void selectCaptureFrameSig(const QImage &rgbImage);

    void selectCaptureSig();//外部调用，用于触发selectCaptureSlot()槽在子线程中执行

public slots:
    void selectCaptureSlot();
    
```

## 参考资料
1. [嵌入式LINUX环境下视频采集知识(V4L2)](http://blog.chinaunix.net/uid-11765716-id-2855735.html)  
2. [和菜鸟一起学linux之V4L2摄像头应用流程](https://blog.csdn.net/eastmoon502136/article/details/8190262)  
3. [嵌入式图像处理算法优化指南之YUV转RGB的高效实现](http://blog.sina.com.cn/s/blog_1368ebb6d0102vujd.html)  
4. [YUV到RGB颜色空间转换算法研究](https://wenku.baidu.com/view/f57562ec04a1b0717fd5ddae.html)  
5. [YUV图解(YUV444, YUV422, YUV420, YV12, NV12, NV21)](https://blog.csdn.net/xjhhjx/article/details/80291465)  
6. [YUV图像格式存储方式(YUV420_NV12、YUV420_NV21、YUV422_YUYV、YUV422_YUY2)](https://blog.csdn.net/u012633319/article/details/95669597)

## 作者联系方式
**邮箱:justdoit_mqr@163.com**  
**新浪微博:@为-何-而来**  
