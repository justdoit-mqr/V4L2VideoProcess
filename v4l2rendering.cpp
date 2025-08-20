/****************************************************************************
*
* Copyright (C) 2019-2025 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2024.05.17
 *@brief:   负责渲染处理V4L2帧数据(基于opengl的api)
 */
#include "v4l2rendering.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>

/*
 *@brief:  构造函数
 *@date:   2024.05.17
 *@param:  pixel_format:帧格式(使用v4l2的宏)
 *@param:  pixel_width:像素宽度(需要确保为偶数)  pixel_height:像素高度
 *@param:  is_tv_range:true=TV Range   false=FULL Range
 */
V4l2Rendering::V4l2Rendering(uint pixel_format, uint pixel_width, uint pixel_height, bool is_tv_range, QObject *parent)
    : QObject(parent),pixelFormat(pixel_format),pixelWidth(pixel_width),pixelHeight(pixel_height),isTVRange(is_tv_range),
    texture1(QOpenGLTexture::Target2D),texture2(QOpenGLTexture::Target2D),texture3(QOpenGLTexture::Target2D)
{

}
/*
 *@brief:  析构函数，释放资源
 *@date:   2025.08.20
 */
V4l2Rendering::~V4l2Rendering()
{
    if(FBO)
    {
        delete FBO;
    }
}
/*
 *@brief:  建立OpenGL的资源和状态
 *注：通常情况下该函数仅会在第一次调用resize事件时会被调用一次，但在我们的RK3568(Qt5.15.8)上实测发现会被调用两次。
 *通过分析Qt源代码发现在resize事件触发opengl初始化后，在show事件处理时会判断内部QOpenGLContext的共享上下文是否与窗体
 *的共享上下文一致，如果不一致会重新初始化。我们的板子运行会初始化两次，并且实测两次初始化环境的当前上下文是不同的。根据show
 *事件的源代码处理可知，如果设置了“QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);”则初始化只会执行
 *一次，不过该操作意味着应用程序中不同的顶层窗口之间也能共享上下文。如果不想这么做，则可以优化initializeGL()函数内部的处理
 *使其支持多次调用，这也是目前采取的方案(关键点是在initTextures()函数中处理纹理对象的销毁和创建)。
 *@date:   2024.05.17
 */
void V4l2Rendering::initializeGL()
{
    isInitGl = true;

    initializeOpenGLFunctions();//绑定QOpenGLFunctions的上下文
    glClearColor(0.0f,0.0f,0.0f,1.0f);//设置清屏颜色
    glClear(GL_COLOR_BUFFER_BIT);//清空颜色缓冲区

    /*0.关联上下文的销毁信号，用来销毁OpenGL纹理对象*/
    connect(QOpenGLContext::currentContext(),&QOpenGLContext::aboutToBeDestroyed,
            this,&V4l2Rendering::destroyTexture,Qt::DirectConnection);

    /*1.初始化VAO、VBO和FBO*/
    VAO.create();//创建顶点数组对象(向GPU申请创建)
    VAO.bind();//将顶点数组对象绑定到opengl绑定点，直到release保存着顶点数据状态的所有修改
    VBO.create();//创建缓存对象(向GPU申请创建)，本质是一段可供使用的GPU内存
    VBO.bind();//绑定到当前顶点缓存区
    VBO.setUsagePattern(QOpenGLBuffer::StaticDraw);//设置为一次修改，多次使用(坐标不变,变得只是像素点)提高优化效率
    //顶点数据(顶点坐标(3float[-1.0f-1.0f])+纹理坐标(2float[0.0f-1.0f]))
    //opengl标准化设备坐标以屏幕中心为原点，坐标在[-1,1]之间。纹理坐标则以左下角为原点，范围为[0,1]
    //纹理坐标的Y方向需要是反的,因为纹理坐标以左下角为原点，而显示纹理以左上角为坐标原点
    float vertices[] = {
        //顶点坐标            //纹理坐标
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,        //左下
        1.0f,  -1.0f, 0.0f,  1.0f, 1.0f,        //右下
        -1.0f, 1.0f,  0.0f,  0.0f, 0.0f,        //左上
        1.0f,  1.0f,  0.0f,  1.0f, 0.0f         //右上
    };
    VBO.allocate(vertices,sizeof(vertices));//分配显存大小，并绑定顶点数据到缓存区
    //初始化FBO
    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);//2D渲染，不需要深度和模板测试
    fboFormat.setSamples(0);//视频帧非几何渲染，不使用多重采样
    if(FBO)
    {
        delete FBO;
        FBO = new QOpenGLFramebufferObject(pixelWidth,pixelHeight,fboFormat);
    }

    /* 2.初始化着色器
     * 使用GLSL语言编写的顶点着色器和片段着色器程序，集成了部分yuv格式的转换处理。
     * 注:OpenGL (ES) 2.0以前的版本仅支持固定管线，不支持可编程管线(GLSL着色器)，所以要确保硬件支持OpenGL 2.0及以上版本*/
    initVertexShader();
    initFragmentShader();

    /*3.初始化纹理*/
    initTexture();

    /*4.初始化着色器程序*/
    initShaderProgram();

    VBO.release();
    VAO.release();
}
/*
 *@brief:  设置OpenGL的视口、投影
 *@date:   2024.05.17
 *@param:  w:宽  h:高
 */
void V4l2Rendering::resizeGL(int w, int h)
{
    this->widgetWidth = w;
    this->widgetHeight = h;
    glViewport(0,0,w,h);
}
/*
 *@brief:  渲染OpenGL场景
 *@date:   2024.05.17
 */
void V4l2Rendering::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);//防止叠图

    //仅在纹理对象有效(setData)的情况下才绘制纹理
    if(isVaildTexture)
    {
        if(needCaptureImage)
        {
            //绑定FBO，将本次绘制渲染到帧缓冲对象上
            FBO->bind();
            glViewport(0,0,pixelWidth,pixelHeight);//FBO使用帧的原始像素大小，需要调整gl视图
            //在FBO上绘制纹理
            paintGLTexture();
            //将FBO转换成QImage并通过信号发射出去，参数true表示对OpenGL坐标进行翻转到光栅坐标
            QImage image = FBO->toImage(true);
            emit captureImageSig(image);
            //释放FBO绑定，重置采集标识
            FBO->release();
            needCaptureImage = false;
            glViewport(0,0,widgetWidth,widgetHeight);//恢复成组件的视图大小
        }
        //执行直接渲染到显示组件
        paintGLTexture();
    }
}
/*
 *@brief:  设置镜像调整参数
 *@date:   2025.08.14
 *@param:  hMirror:true=水平镜像
 *@param:  vMirror:true=垂直镜像
 */
void V4l2Rendering::setMirrorParam(const bool &hMirror, const bool &vMirror)
{
    if(mirrorParam.hMirror != hMirror)
    {
        mirrorParam.hMirror = hMirror;
        mirrorParamChanged = true;
    }
    if(mirrorParam.vMirror != vMirror)
    {
        mirrorParam.vMirror = vMirror;
        mirrorParamChanged = true;
    }
}
/*
 *@brief:  设置颜色调整参数
 *@date:   2025.08.14
 *@param:  enableColorAdjust:是否使能基础颜色调整
 *@param:  brightness:亮度调整  典型范围[0.5,1.5]
 *@param:  contrast:对比度调整  典型范围[0.5,1.5]
 *@param:  saturation:饱和度调整  典型范围[0.0,2.0]
 */
void V4l2Rendering::setColorAdjustParam(const bool &enableColorAdjust, const float &brightness,
                                        const float &contrast, const float &saturation)
{
    if(colorAdjustParam.enableColorAdjust != enableColorAdjust)
    {
        colorAdjustParam.enableColorAdjust = enableColorAdjust;
        colorAdjustParamChanged = true;
    }
    if(colorAdjustParam.brightness != brightness)
    {
        colorAdjustParam.brightness = brightness;
        colorAdjustParamChanged = true;
    }
    if(colorAdjustParam.contrast != contrast)
    {
        colorAdjustParam.contrast = contrast;
        colorAdjustParamChanged = true;
    }
    if(colorAdjustParam.saturation != saturation)
    {
        colorAdjustParam.saturation = saturation;
        colorAdjustParamChanged = true;
    }
}
/*
 *@brief:  更新(渲染)v4l2帧数据
 *注：此处调用QOpenGLTexture的setData时，参数PixelFormat需要与initTexture()中的format保持一致，初期使用Red、RG、RGB、RGBA，
 *后面为了与LuminanceFormat等对应起来，改用Luminance、LuminanceAlpha、RGB、RGBA。
 *@date:   2024.05.17
 *@param:  v4l2FrameData:v4l2帧二维指针(指针数组)，planes根据pixelFormat格式在内部自动确定
 *对于多平面planes每一个元素对应着一个平面(不连续)v4l2FrameData[0]即完整的yuv数据
 */
void V4l2Rendering::updateV4l2Frame(uchar **v4l2FrameData)
{
    //确保已经初始化opengl相关资源(initializeGL)，否则直接操作纹理会出错
    if(!this->isInitGl)
    {
        return;
    }
    isVaildTexture = true;
    //one planes格式，设置两个纹理对象数据
    if(pixelFormat == V4L2_PIX_FMT_YUYV ||
            pixelFormat == V4L2_PIX_FMT_YVYU)
    {
        //纹理对象能够根据size自动读取对应字节的数据
        texture1.setData(QOpenGLTexture::LuminanceAlpha, QOpenGLTexture::UInt8, v4l2FrameData[0],&pixelTransferOptions1);
        texture2.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, v4l2FrameData[0],&pixelTransferOptions2);
    }
    //two planes格式，设置两个纹理对象数据
    else if(pixelFormat == V4L2_PIX_FMT_NV12 ||
            pixelFormat == V4L2_PIX_FMT_NV21)
    {
        //纹理对象能够根据size自动读取对应字节的数据
        texture1.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, v4l2FrameData[0],&pixelTransferOptions1);
        texture2.setData(QOpenGLTexture::LuminanceAlpha, QOpenGLTexture::UInt8, v4l2FrameData[0]+pixelWidth*pixelHeight,&pixelTransferOptions2);
    }
    //three planes格式，设置三个纹理对象数据
    else if(pixelFormat == V4L2_PIX_FMT_YUV420 ||
            pixelFormat == V4L2_PIX_FMT_YVU420)
    {
        //纹理对象能够根据size自动读取对应字节的数据
        texture1.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, v4l2FrameData[0],&pixelTransferOptions1);
        texture2.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, v4l2FrameData[0]+pixelWidth*pixelHeight,&pixelTransferOptions2);
        texture3.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, v4l2FrameData[0]+pixelWidth*pixelHeight*5/4,&pixelTransferOptions3);
    }
}
/*
 *@brief:  初始化顶点着色器
 *@date:   2024.05.17
 */
void V4l2Rendering::initVertexShader()
{
    /*顶点着色器，两个输入(顶点坐标(vec3)+纹理坐标(vec2)),其中顶点坐标转成vec4传给内置变量。
     *两个uniform全局变量(在外部传递镜像参数)，一个输出(纹理坐标，根据镜像参数将输入的纹理坐标
     *进行处理后传给该输出)传递给下一个着色器*/
    vertexShader = QString("attribute vec3 aPos;\n"
                           "attribute vec2 aTexCoord;\n"
                           "varying vec2 TexCoord;\n"
                           "\n"
                           "uniform bool hMirror;\n"
                           "uniform bool vMirror;\n"
                           "\n"
                           "void main(){\n"
                           "vec2 adjustedTexCoord = aTexCoord;\n"
                           "\n"
                           "if(hMirror){adjustedTexCoord.x = 1.0 - adjustedTexCoord.x;}\n"
                           "if(vMirror){adjustedTexCoord.y = 1.0 - adjustedTexCoord.y;}\n"
                           "\n"
                           "gl_Position = vec4(aPos, 1.0);\n"
                           "TexCoord = adjustedTexCoord;\n"
                           "}\n");

    //获取当前的glsl版本，声明到着色器中
    QString glslVersionStr = QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    QRegularExpression reg("\\d+[.]\\d+");
    QRegularExpressionMatch expMatch;
    if(glslVersionStr.lastIndexOf(reg,-1,&expMatch) != -1)
    {
        int glslVersion = expMatch.captured().toDouble()*1000/10;
        //glsl从1.3版本开始，废弃了attribute关键字(以及varying关键字)，属性变量统一用in/out作为前置关键字
        if(glslVersion >= 130)
        {
            vertexShader.replace("attribute","in");
            vertexShader.replace("varying","out");
        }

        //添加version头
        if(glslVersionStr.contains("ES",Qt::CaseInsensitive))
        {
            if(glslVersion >= 300)
            {
                //仅OpenGL ES 3.0及以上添加版本头
                vertexShader.prepend(QString("#version %1 es\n").arg(glslVersion));
            }
        }
        else
        {
            vertexShader.prepend(QString("#version %1\n").arg(glslVersion));
        }
    }
    //qDebug().noquote()<<"vertexShader:\n"<<vertexShader;
}
/*
 *@brief:  初始化片段着色器
 *注：此处片段着色器内部使用的YUV转RGB(包括饱和度算法的亮度系数)为BT.709标准的Full range格式的转换公式(如果标识了TV Range，内部会自动
 *将其转换为FULL Range)，根据不同的YUV格式实现不同的处理。
 *实测对于BT.601的数据按照BT.709方式处理和BT.601的方式处理，显示效果没有太大的差距，为了着色器程序的处理性能，此处不再根据全局变量动态区分两
 *种标准，而是均使用BT.709标准处理。
 *@date:   2024.05.17
 */
void V4l2Rendering::initFragmentShader()
{
    /*此处定义片段着色器，不同的帧格式片段着色器程序代码略有不同，所以这里通过判断条件分支，设置不同的着色器字符串。
     *以下着色器程序的基本组成一般包括：
     *一个输入(TexCoord纹理坐标，来自与顶点着色器的输出);若干个uniform全局变量(包括若干纹理采样器(在外部绑定纹理及通道)和TV Range标志
     *参数(着色器程序默认按照Full Range进行归一化处理，如果是纹理采样输入是TV Range，需要先转换成Full Range)，以及若干颜色调整参数，这
     *些参数在外部传递);一个输出(vec4,片段纹理颜色，低版本为内置的gl_FragColor,高版本需要通过out vec4 FragColor自定义输出);
     *通过采样器获取对应通道的纹理数据(可以通过插值匹配计算纹理坐标区域各点的值)，根据TV Range标志确定是否转换，然后再通过yuv转rgb公式，通
     *过矩阵计算对应点的rgb数据输出。最后根据外部设置的颜色调整参数，选择是否对rgb进一步处理(亮度、对比度、饱和度等基础的颜色调节)，以及在图
     *像输出之前做一次伽马校正，该操作针对人眼感知的明暗处理上效果非常好，比较适合对摄像头采集数据处理，但相对也消耗一些性能。*/

    /*one planes格式，需要两个纹理采样器(两通道取y+四通道取uv)*/
    if(pixelFormat == V4L2_PIX_FMT_YUYV || pixelFormat == V4L2_PIX_FMT_YVYU)
    {
        QString tmpShader = QString("varying vec2 TexCoord;\n\n"
                                    //纹理采样及TV Range转换标志
                                    "uniform bool isTvRange;\n"
                                    "uniform sampler2D texY;\n"
                                    "uniform sampler2D texUV;\n\n"
                                    //颜色调整参数
                                    "uniform bool enableColorAdjust;\n"
                                    "uniform float brightness;\n"
                                    "uniform float contrast;\n"
                                    "uniform float saturation;\n\n"
                                    //主函数
                                    "void main(){\n"
                                    //yuv采样转换为rgb
                                    "float y = texture2D(texY, TexCoord).r;\n"
                                    "vec2 uv = texture2D(texUV, TexCoord).ga;\n"
                                    "if(isTvRange){\n"//TV Range转换为FULL Range
                                    "y = (y*255.0-16.0)/(235.0-16.0);\n"
                                    "uv = (uv*255.0-vec2(16.0,16.0))/(240.0-16.0);\n"
                                    "}\n"
                                    "uv = uv - vec2(0.5,0.5);\n"
                                    "float u = uv.r;\n"
                                    "float v = uv.g;\n"
                                    "vec3 yuv = vec3(y, %1, %2);\n"
                                    "vec3 rgb = mat3(1.0,      1.0,      1.0,"
                                                    "0.0,      -0.187324,  1.8556,"
                                                    "1.5748,   -0.468124,   0.0) * yuv;\n\n"
                                    //rgb颜色调整
                                    "if(enableColorAdjust){\n"
                                    "rgb = (rgb - 0.5) * contrast + 0.5;\n"
                                    "vec3 luminanceVec = vec3(dot(rgb, vec3(0.2126, 0.7152, 0.0722)));\n"
                                    "rgb = luminanceVec + (rgb - luminanceVec) * saturation;\n"
                                    "rgb = rgb * brightness;\n"
                                    "}\n\n"
                                    //rgb校正及输出
                                    "rgb = clamp(rgb,0.0,1.0);\n"
                                    "rgb = pow(rgb, vec3(1.0/2.2));\n"//伽马校正
                                    "gl_FragColor = vec4(rgb, 1.0);\n"
                                    "}\n");
        //格式UV顺序不同
        if(pixelFormat == V4L2_PIX_FMT_YUYV)
        {
            fragmentShader = tmpShader.arg("u","v");
        }
        else if(pixelFormat == V4L2_PIX_FMT_YVYU)
        {
            fragmentShader = tmpShader.arg("v","u");
        }
    }
    /*two planes格式，需要两个纹理采样器*/
    else if(pixelFormat == V4L2_PIX_FMT_NV12 || pixelFormat == V4L2_PIX_FMT_NV21)
    {
        QString tmpShader = QString("varying vec2 TexCoord;\n\n"
                                    //纹理采样及TV Range转换标志
                                    "uniform bool isTvRange;\n"
                                    "uniform sampler2D texY;\n"
                                    "uniform sampler2D texUV;\n\n"
                                    //颜色调整参数
                                    "uniform bool enableColorAdjust;\n"
                                    "uniform float brightness;\n"
                                    "uniform float contrast;\n"
                                    "uniform float saturation;\n\n"
                                    //主函数
                                    "void main(){\n"
                                    //yuv采样转换为rgb
                                    "float y = texture2D(texY, TexCoord).r;\n"
                                    "vec2 uv = texture2D(texUV, TexCoord).ra;\n"
                                    "if(isTvRange){\n"//TV Range转换为FULL Range
                                    "y = (y*255.0-16.0)/(235.0-16.0);\n"
                                    "uv = (uv*255.0-vec2(16.0,16.0))/(240.0-16.0);\n"
                                    "}\n"
                                    "uv = uv - vec2(0.5, 0.5);\n"
                                    "float u = uv.r;\n"
                                    "float v = uv.g;\n"
                                    "vec3 yuv = vec3(y, %1, %2);\n"
                                    "vec3 rgb = mat3(1.0,      1.0,      1.0,"
                                                    "0.0,      -0.187324,  1.8556,"
                                                    "1.5748,   -0.468124,   0.0) * yuv;\n\n"
                                    //rgb颜色调整
                                    "if(enableColorAdjust){\n"
                                    "rgb = (rgb - 0.5) * contrast + 0.5;\n"
                                    "vec3 luminanceVec = vec3(dot(rgb, vec3(0.2126, 0.7152, 0.0722)));\n"
                                    "rgb = luminanceVec + (rgb - luminanceVec) * saturation;\n"
                                    "rgb = rgb * brightness;\n"
                                    "}\n\n"
                                    //rgb校正及输出
                                    "rgb = clamp(rgb,0.0,1.0);\n"
                                    "rgb = pow(rgb, vec3(1.0/2.2));\n"//伽马校正
                                    "gl_FragColor = vec4(rgb, 1.0);\n"
                                    "}\n");
        //格式UV顺序不同
        if(pixelFormat == V4L2_PIX_FMT_NV12)
        {
            fragmentShader = tmpShader.arg("u","v");
        }
        else if(pixelFormat == V4L2_PIX_FMT_NV21)
        {
            fragmentShader = tmpShader.arg("v","u");
        }
    }
    /*three planes格式，需要三个纹理采样器*/
    else if(pixelFormat == V4L2_PIX_FMT_YUV420 || pixelFormat == V4L2_PIX_FMT_YVU420)
    {
        QString tmpShader = QString("varying vec2 TexCoord;\n\n"
                                    //纹理采样及TV Range转换标志
                                    "uniform bool isTvRange;\n"
                                    "uniform sampler2D texY;\n"
                                    "uniform sampler2D texU;\n"
                                    "uniform sampler2D texV;\n\n"
                                    //颜色调整参数
                                    "uniform bool enableColorAdjust;\n"
                                    "uniform float brightness;\n"
                                    "uniform float contrast;\n"
                                    "uniform float saturation;\n\n"
                                    //主函数
                                    "void main(){\n"
                                    //yuv采样转换为rgb
                                    "float y = texture2D(texY, TexCoord).r;\n"
                                    "float u = texture2D(texU, TexCoord).r - 0.5;\n"
                                    "float v = texture2D(texV, TexCoord).r - 0.5;\n"
                                    "if(isTvRange){\n"//TV Range转换为FULL Range
                                    "y = (y*255.0-16.0)/(235.0-16.0);\n"
                                    "u = (u*255.0-16.0)/(240.0-16.0);\n"
                                    "v = (v*255.0-16.0)/(240.0-16.0);\n"
                                    "}\n"
                                    "vec3 yuv = vec3(y, %1, %2);\n"
                                    "vec3 rgb = mat3(1.0,      1.0,      1.0,"
                                                    "0.0,      -0.187324,  1.8556,"
                                                    "1.5748,   -0.468124,   0.0) * yuv;\n\n"
                                    //rgb颜色调整
                                    "if(enableColorAdjust){\n"
                                    "rgb = (rgb - 0.5) * contrast + 0.5;\n"
                                    "vec3 luminanceVec = vec3(dot(rgb, vec3(0.2126, 0.7152, 0.0722)));\n"
                                    "rgb = luminanceVec + (rgb - luminanceVec) * saturation;\n"
                                    "rgb = rgb * brightness;\n"
                                    "}\n\n"
                                    //rgb校正及输出
                                    "rgb = clamp(rgb,0.0,1.0);\n"
                                    "rgb = pow(rgb, vec3(1.0/2.2));\n"//伽马校正
                                    "gl_FragColor = vec4(rgb, 1.0);\n"
                                    "}\n");
        //格式UV顺序不同
        if(pixelFormat == V4L2_PIX_FMT_YUV420)
        {
            fragmentShader = tmpShader.arg("u","v");
        }
        else if(pixelFormat == V4L2_PIX_FMT_YVU420)
        {
            fragmentShader = tmpShader.arg("v","u");
        }
    }
    else
    {
        qDebug()<<QString("YuvRenderingWidget:not support current pixelFormat:%1%2%3%4!")
                  .arg((char)(pixelFormat&0xff))
                  .arg((char)((pixelFormat>>8)&0xff))
                  .arg((char)((pixelFormat>>16)&0xff))
                  .arg((char)((pixelFormat>>24)&0xff));
    }

    //获取当前的glsl版本，声明到着色器中
    QString glslVersionStr = QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
    QRegularExpression reg("\\d+[.]\\d+");
    QRegularExpressionMatch expMatch;
    if(glslVersionStr.lastIndexOf(reg,-1,&expMatch) != -1)
    {
        int glslVersion = expMatch.captured().toDouble()*1000/10;

        //glsl从1.3版本开始，废弃了attribute关键字(以及varying关键字)，属性变量统一用in/out作为前置关键字
        //且高版本取消了内置gl_FragColor变量，需要自己定义，texture2D换成了texture
        if(glslVersion >= 130)
        {
            fragmentShader.prepend("out vec4 FragColor;");
            fragmentShader.replace("varying","in");
            fragmentShader.replace("texture2D","texture");
            fragmentShader.replace("gl_FragColor","FragColor");
        }

        //添加version头
        if(glslVersionStr.contains("ES",Qt::CaseInsensitive))
        {
            if(glslVersion >= 300)
            {
                //OpenGL ES 3.0及以上加上版本标识ES，并指定精度(ES的特有操作)
                fragmentShader.prepend(QString("#version %1 es\nprecision mediump float;\n").arg(glslVersion));
            }
            else
            {
                //OpenGL ES 2.0不用加版本头，仅指定精度(ES的特有操作)
                fragmentShader.prepend(QString("precision mediump float;\n"));
            }
        }
        else
        {
            fragmentShader.prepend(QString("#version %1\n").arg(glslVersion));
        }
    }
    //qDebug().noquote()<<"fragmentShader:\n"<<fragmentShader;
}
/*
 *@brief:  初始化纹理对象
 *注:在初始化QOpenGLTexture纹理对象时，setFormat的参数很关键，之前在PC端(OpenGL)使用R8_UNorm、RG8_UNorm、RGB8_UNorm、RGBA8_UNorm
 *设置1-4通道的格式，运行没有问题。但在arm端(OpenGL es 2.0)怎么都出不来图像，经过排查确定是纹理对象的问题，好像是因为opengl es 2.0比较特殊
 *所以专门定义了一组LuminanceFormat、LuminanceAlphaFormat(着色器中对应.ra)、RGBFormat、RGBAFormat来适配1-4通道格式，并且实测PC端也可以兼容该格式。
 *@date:   2024.05.17
 */
void V4l2Rendering::initTexture()
{
    //one planes格式，需要两个纹理(Y,UV)
    if(pixelFormat == V4L2_PIX_FMT_YUYV ||
            pixelFormat == V4L2_PIX_FMT_YVYU)
    {
        //纹理1对应y分量通道
        texture1.setSize(pixelWidth,pixelHeight);
        texture1.setFormat(QOpenGLTexture::LuminanceAlphaFormat);//双通道格式(每两个字节提取一个Y)
        texture1.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture1.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture1.allocateStorage(QOpenGLTexture::LuminanceAlpha,QOpenGLTexture::UInt8);
        //纹理2对应uv分量通道
        texture2.setSize(pixelWidth/2,pixelHeight);//yuv422格式，uv宽除以2，高不变
        texture2.setFormat(QOpenGLTexture::RGBAFormat);//四通道格式(每四个字节提取一组UV)
        texture2.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture2.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture2.allocateStorage(QOpenGLTexture::RGBA,QOpenGLTexture::UInt8);
    }
    //two planes格式，需要两个纹理(Y,UV)
    else if(pixelFormat == V4L2_PIX_FMT_NV12 ||
            pixelFormat == V4L2_PIX_FMT_NV21)
    {
        //纹理1对应y分量通道
        texture1.setSize(pixelWidth,pixelHeight);
        texture1.setFormat(QOpenGLTexture::LuminanceFormat);//单通道格式
        texture1.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture1.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture1.allocateStorage(QOpenGLTexture::Luminance,QOpenGLTexture::UInt8);
        if(pixelWidth%4)
        {
            //默认4字节对齐，当行像素字节不是4的整数倍时，改变为2字节对齐(前提确保pixelWidth为偶数)
            pixelTransferOptions1.setAlignment(2);
        }
        //纹理2对应uv分量通道
        texture2.setSize(pixelWidth/2,pixelHeight/2);//yuv420格式，uv宽高均除以2
        texture2.setFormat(QOpenGLTexture::LuminanceAlphaFormat);//双通道格式(每一个点有两个数据)
        texture2.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture2.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture2.allocateStorage(QOpenGLTexture::LuminanceAlpha,QOpenGLTexture::UInt8);
        if(pixelWidth%4)
        {
            //默认4字节对齐，当行像素字节不是4的整数倍时，改变为2字节对齐(前提确保pixelWidth为偶数)
            pixelTransferOptions2.setAlignment(2);
        }
    }
    //three planes格式，需要三个纹理(Y,U，V)
    else if(pixelFormat == V4L2_PIX_FMT_YUV420 ||
            pixelFormat == V4L2_PIX_FMT_YVU420)
    {
        //纹理1对应y分量通道
        texture1.setSize(pixelWidth,pixelHeight);
        texture1.setFormat(QOpenGLTexture::LuminanceFormat);//单通道格式
        texture1.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture1.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture1.allocateStorage(QOpenGLTexture::Luminance,QOpenGLTexture::UInt8);
        if(pixelWidth%4)
        {
            //默认4字节对齐，当行像素字节不是4的整数倍时，改变为2字节对齐(前提确保pixelWidth为偶数)
            pixelTransferOptions1.setAlignment(2);
        }
        //纹理2对应u/v分量通道
        texture2.setSize(pixelWidth/2,pixelHeight/2);//yuv420格式，uv宽高均除以2
        texture2.setFormat(QOpenGLTexture::LuminanceFormat);//单通道格式
        texture2.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture2.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture2.allocateStorage(QOpenGLTexture::Luminance,QOpenGLTexture::UInt8);
        //纹理3对应u/v分量通道
        texture3.setSize(pixelWidth/2,pixelHeight/2);//yuv420格式，uv宽高均除以2
        texture3.setFormat(QOpenGLTexture::LuminanceFormat);//单通道格式
        texture3.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture3.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture3.allocateStorage(QOpenGLTexture::Luminance,QOpenGLTexture::UInt8);
        if((pixelWidth/2)%4)//纹理2和纹理3像素宽度和通道格式一致，对齐方式也保持一致即可
        {
            if((pixelWidth/2)%2)
            {
                //默认4字节对齐，当行像素字节不是4的整数倍时，且不是2的整数倍，改变为1字节对齐
                pixelTransferOptions2.setAlignment(1);
                pixelTransferOptions3.setAlignment(1);
            }
            else
            {
                //默认4字节对齐，当行像素字节不是4的整数倍时，改变为2字节对齐(前提确保pixelWidth/2为偶数)
                pixelTransferOptions2.setAlignment(2);
                pixelTransferOptions3.setAlignment(2);
            }
        }
    }
}
/*
 *@brief:  初始化着色器程序
 *@date:   2025.08.20
 */
void V4l2Rendering::initShaderProgram()
{
    shaderProgram.removeAllShaders();
    shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex,vertexShader);//附加顶点着色器和片段着色器
    shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment,fragmentShader);
    shaderProgram.link();//链接着色器
    shaderProgram.bind();
    //使用VBO为数据源，设置解析格式,应用到VAO中
    //第一个参数为VAO的属性名/location位置索引，与顶点着色器的输入变量关联;
    //因为location需要在着色器中通过layout (location = *)指定，而该语法在glsl 330版本才提供,为了兼容旧版本，此处使用属性名
    shaderProgram.setAttributeBuffer("aPos", GL_FLOAT, 0, 3, 5 * sizeof(float));
    shaderProgram.setAttributeBuffer("aTexCoord", GL_FLOAT, 3*sizeof(float), 2, 5 * sizeof(float));
    //使能VAO指定名称/location位置索引的属性变量,此处使用属性名，原因同上
    shaderProgram.enableAttributeArray("aPos");//顶点着色器的顶点坐标信息，意味着opengl使用顶点坐标绘制图形
    shaderProgram.enableAttributeArray("aTexCoord");//顶点着色器的纹理坐标信息，意味着opengl使用纹理坐标来对纹理进行映射，实现纹理贴图效果
    //为顶点着色器传递初始化的镜像参数
    shaderProgram.setUniformValue("hMirror",mirrorParam.hMirror);
    shaderProgram.setUniformValue("vMirror",mirrorParam.vMirror);
    //为片段着色器传递初始化的isTVRange参数和颜色调整参数
    shaderProgram.setUniformValue("isTvRange",isTVRange);
    shaderProgram.setUniformValue("enableColorAdjust",colorAdjustParam.enableColorAdjust);
    shaderProgram.setUniformValue("brightness",colorAdjustParam.brightness);
    shaderProgram.setUniformValue("contrast",colorAdjustParam.contrast);
    shaderProgram.setUniformValue("saturation",colorAdjustParam.saturation);
}
/*
 *@brief:  基于着色器程序和VAO的操作流程，绘制纹理
 *@date:   2025.08.20
 */
void V4l2Rendering::paintGLTexture()
{
    //绑定着色器程序和VAO
    shaderProgram.bind();
    VAO.bind();

    //为顶点着色器传递新的镜像参数
    if(mirrorParamChanged)
    {
        shaderProgram.setUniformValue("hMirror",mirrorParam.hMirror);
        shaderProgram.setUniformValue("vMirror",mirrorParam.vMirror);

        colorAdjustParamChanged = false;
    }
    //为片段着色器传递新的颜色调整参数
    if(colorAdjustParamChanged)
    {
        shaderProgram.setUniformValue("enableColorAdjust",colorAdjustParam.enableColorAdjust);
        shaderProgram.setUniformValue("brightness",colorAdjustParam.brightness);
        shaderProgram.setUniformValue("contrast",colorAdjustParam.contrast);
        shaderProgram.setUniformValue("saturation",colorAdjustParam.saturation);

        colorAdjustParamChanged = false;
    }
    //绘制纹理
    drawTexture();

    //释放着色器程序和VAO
    shaderProgram.release();
    VAO.release();
}
/*
 *@brief:  绘制纹理(流程：绑定纹理对象(关联着色器采样器)->绘制纹理->释放绑定）
 *该函数要在paintGL()中调用，除了有必须的绘制纹理操作，还需要不断的绑定纹理单元，否则可能出现异常
 *@date:   2024.05.17
 */
void V4l2Rendering::drawTexture()
{
    //处理两个纹理对象
    if(pixelFormat == V4L2_PIX_FMT_YUYV ||
            pixelFormat == V4L2_PIX_FMT_YVYU ||
            pixelFormat == V4L2_PIX_FMT_NV12 ||
            pixelFormat == V4L2_PIX_FMT_NV21)
    {
        //将纹理对象绑定到对应的纹理单元索引
        texture1.bind(0);
        texture2.bind(1);
        //绑定片段着色器的采样器到对应的纹理单元(对象)
        shaderProgram.setUniformValue("texY", 0);
        shaderProgram.setUniformValue("texUV", 1);
        //绘制纹理
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        //释放绑定
        texture1.release();
        texture2.release();
    }
    //处理三个纹理对象
    else if(pixelFormat == V4L2_PIX_FMT_YUV420 ||
            pixelFormat == V4L2_PIX_FMT_YVU420)
    {
        //将纹理对象绑定到对应的纹理单元索引
        texture1.bind(0);
        texture2.bind(1);
        texture3.bind(2);
        //绑定片段着色器的采样器到对应的纹理单元(对象)
        shaderProgram.setUniformValue("texY", 0);
        shaderProgram.setUniformValue("texU", 1);
        shaderProgram.setUniformValue("texV", 2);
        //绘制纹理
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        //释放绑定
        texture1.release();
        texture2.release();
        texture3.release();
    }
}
/*
 *@brief:  销毁OpenGL纹理对象
 *销毁纹理对象必须在创建纹理对象的上下文中，所以将该函数关联QOpenGLContext::aboutToBeDestroyed信号。
 *正常情况下在析构函数中随着QOpenGLContext对象的销毁，OpenGL纹理对象也会跟着销毁，所以不需要单独处理。但是我们的板子遇到了
 *执行两次initializeGL()的情况，且第一次初始化的上下文很快就销毁了，而在该上下文中创建的纹理对象必须通过信号去销毁，否则将导致
 *第二次初始化的上下文无法使用创建好的纹理对象，又无法销毁。
 *@date:   2024.05.17
 */
void V4l2Rendering::destroyTexture()
{
    if(texture1.isCreated())
    {
       texture1.destroy();
    }
    if(texture2.isCreated())
    {
       texture2.destroy();
    }
    if(texture3.isCreated())
    {
       texture3.destroy();
    }
}
