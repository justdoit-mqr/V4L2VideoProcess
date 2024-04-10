/****************************************************************************
*
* Copyright (C) 2019-2024 MiaoQingrui. All rights reserved.
* Author: 缪庆瑞 <justdoit_mqr@163.com>
*
****************************************************************************/
/*
 *@author:  缪庆瑞
 *@date:    2024.04.05
 *@brief:   直接渲染显示yuv数据的部件(基于opengl的api)
 */
#include "yuvrenderingwidget.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTimer>
#include <QFile>

/*
 *@brief:  构造函数
 *@date:   2024.04.05
 *@param:  pixel_format:帧格式(使用v4l2的宏)
 *@param:  pixel_width:像素宽度  pixel_height:像素高度
 */
YuvRenderingWidget::YuvRenderingWidget(uint pixel_format, uint pixel_width, uint pixel_height, QWidget *parent)
    : QOpenGLWidget(parent),pixelFormat(pixel_format),pixelWidth(pixel_width),pixelHeight(pixel_height),
    texture1(QOpenGLTexture::Target2D),texture2(QOpenGLTexture::Target2D),texture3(QOpenGLTexture::Target2D)
{

}

YuvRenderingWidget::~YuvRenderingWidget()
{
    makeCurrent();
}
/*
 *@brief:  该接口用于功能测试，通过读取yuv文件测试该类的渲染功能
 *注:可使用FFmpeg工具将mp4格式文件转换成yuv文件进行测试，例如“ffmpeg -i test.mp4 -an -pix_fmt nv12 -s 1024x576 nv12.yuv”
 *FFmpeg支持的格式可通过“ffmpeg -pix_fmts”列出。
 *@date:   2024.04.09
 *@param:  file:需要读取的yuv文件
 */
void YuvRenderingWidget::readYuvFileTest(QString file)
{
    QFile *yuvFile = new QFile(file,this);
    if(yuvFile->open(QIODevice::ReadOnly))
    {
        QTimer *readYuvFileTimer = new QTimer(this);
        readYuvFileTimer->setInterval(40);
        connect(readYuvFileTimer,&QTimer::timeout,this,[this,yuvFile](){
            if(yuvFile->atEnd())
            {
                yuvFile->seek(0);
            }
            if(pixelFormat == V4L2_PIX_FMT_YUYV ||
                    pixelFormat == V4L2_PIX_FMT_YVYU)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                updateYuvFrameSlot(yuvFrame);
            }
            else if(pixelFormat == V4L2_PIX_FMT_NV12 ||
                    pixelFormat == V4L2_PIX_FMT_NV21)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*3/2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                updateYuvFrameSlot(yuvFrame);
            }
            else if(pixelFormat == V4L2_PIX_FMT_YUV420 ||
                    pixelFormat == V4L2_PIX_FMT_YVU420)
            {
                QByteArray array = yuvFile->read(pixelWidth*pixelHeight*3/2);
                uchar *yuvFrame[1];
                yuvFrame[0] = (uchar *)array.data();
                updateYuvFrameSlot(yuvFrame);
            }
        });
        readYuvFileTimer->start();
    }
    else
    {
        qDebug()<<QString("open yuv file %1 failed!").arg(file);
    }
}
/*
 *@brief:  建立OpenGL的资源和状态
 *在第一次调用resizeGL()或paintGL()之前会被调用一次
 *@date:   2024.04.05
 */
void YuvRenderingWidget::initializeGL()
{
    initializeOpenGLFunctions();//绑定QOpenGLFunctions的上下文
    glClearColor(1.0f,0.0f,0.0f,1.0f);//设置清屏颜色
    glClear(GL_COLOR_BUFFER_BIT);//清空颜色缓冲区

    /*1.初始化VAO和VBO*/
    VAO.create();//创建顶点数组对象
    VAO.bind();//将顶点数组对象绑定到opengl绑定点，直到release保存着顶点数据状态的所有修改
    VBO.create();//创建缓存对象
    VBO.bind();//绑定当前顶点缓存区
    VBO.setUsagePattern(QOpenGLBuffer::StaticDraw);//设置为一次修改，多次使用(坐标不变,变得只是像素点)提高优化效率
    //顶点数据(顶点坐标(3float[-1.0f-1.0f])+纹理坐标(2float[0.0f-1.0f]))
    //纹理坐标的Y方向需要是反的,因为opengl中的坐标系原点为左下角，而显示纹理以左上角为坐标原点
    float vertices[] = {
        //顶点坐标            //纹理坐标
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,        //左下
        1.0f,  -1.0f, 0.0f,  1.0f, 1.0f,        //右下
        -1.0f, 1.0f,  0.0f,  0.0f, 0.0f,        //左上
        1.0f,  1.0f,  0.0f,  1.0f, 0.0f         //右上
    };
    VBO.allocate(vertices,sizeof(vertices));//分配显存大小，并绑定顶点数据到缓存区

    /*2.初始化着色器
     * 使用GLSL语言编写的顶点着色器和片段着色器程序，集成了部分yuv格式的转换处理。
     * 注:OpenGL (ES) 2.0以前的版本仅支持固定管线，不支持可编程管线(GLSL着色器)，所以要确保硬件支持OpenGL 2.0及以上版本
     */
    initVertexShader();
    initFragmentShader();

    /*3.初始化纹理*/
    initTexture();

    /*4.初始化着色器程序*/
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

    VBO.release();
    VAO.release();
}
/*
 *@brief:  设置OpenGL的视口、投影
 *widget大小改变时会被调用
 *@date:   2024.04.05
 *@param:  w:宽  h:高
 */
void YuvRenderingWidget::resizeGL(int w, int h)
{
    glViewport(0,0,w,h);
}
/*
 *@brief:  渲染OpenGL场景
 *widget更新时会被调用
 *@date:   2024.04.05
 */
void YuvRenderingWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);//防止叠图

    //绑定着色器程序和VAO
    shaderProgram.bind();
    VAO.bind();

    //绘制纹理
    drawTexture();

    //释放着色器程序和VAO
    shaderProgram.release();
    VAO.release();
}
/*
 *@brief:  初始化顶点着色器
 *@date:   2024.04.09
 */
void YuvRenderingWidget::initVertexShader()
{
    /*顶点着色器，两个输入(顶点坐标(vec3)+纹理坐标(vec2)),其中顶点坐标转成vec4传给内置变量。
     *一个输出(纹理坐标，此处不经过处理与输入一致)传递给下一个着色器*/
    vertexShader = QString("attribute  vec3 aPos;  "
                           "attribute  vec2 aTexCoord;  "
                           "varying  vec2 TexCoord;  "
                           "\n"
                           "void main()  "
                           "{  "
                           "gl_Position = vec4(aPos, 1.0);  "
                           "TexCoord = aTexCoord;  "
                           "}  ");

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
        vertexShader.prepend(QString("#version %1\n").arg(glslVersion));
    }
    //qDebug()<<"vertexShader:"<<vertexShader;
}
/*
 *@brief:  初始化片段着色器
 *注：此处片段着色器内部使用的yuv转rgb为BT709标准的Full range格式的转换公式，根据不同的yuv格式实现不同的处理
 *@date:   2024.04.09
 */
void YuvRenderingWidget::initFragmentShader()
{
    /*one planes格式，需要两个纹理采样器(两通道取y+四通道取uv)*/
    if(pixelFormat == V4L2_PIX_FMT_YUYV || pixelFormat == V4L2_PIX_FMT_YVYU)
    {
        /*片段着色器，一个输入(纹理坐标)，两个纹理采样器(在外部绑定纹理及通道)，一个输出(vec4,片段纹理颜色,低版本为内置的gl_FragColor,
         *高版本需要通过out vec4 FragColor自定义输出)，通过采样器获取对应通道的纹理数据(可以通过插值匹配计算纹理坐标区域各点的值)，然后
         *通过yuv转rgb公式，通过矩阵计算对应点的rgb数据输出。*/
        QString tmpShader = QString("varying vec2 TexCoord;"
                                    "uniform sampler2D texY;"
                                    "uniform sampler2D texUV;"
                                    ""
                                    "void main()"
                                    "{"
                                    "float y = texture2D(texY, TexCoord).r;"
                                    "vec4 yuyv = texture2D(texUV, TexCoord).rgba - vec4(0,0.5,0,0.5);"
                                    "float u = yuyv.g;"
                                    "float v = yuyv.a;"

                                    "vec3 yuv = vec3(y, %1, %2);"
                                    "vec3 rgb = mat3(1.0,      1.0,      1.0,"
                                                    "0.0,      -0.1868,  1.8556,"
                                                    "1.5748,   -0.4681,   0.0) * yuv;"
                                    "gl_FragColor = vec4(rgb, 1.0);"
                                    "}");
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
        /*片段着色器，一个输入(纹理坐标)，两个纹理采样器(在外部绑定纹理及通道)，一个输出(vec4,片段纹理颜色，低版本为内置的gl_FragColor,
         *高版本需要通过out vec4 FragColor自定义输出)，通过采样器获取对应通道的纹理数据(可以通过插值匹配计算纹理坐标区域各点的值)，然后
         *通过yuv转rgb公式，通过矩阵计算对应点的rgb数据输出。*/
        QString tmpShader = QString("varying vec2 TexCoord;"
                                    "uniform sampler2D texY;"
                                    "uniform sampler2D texUV;"
                                    ""
                                    "void main()"
                                    "{"
                                    "float y = texture2D(texY, TexCoord).r;"
                                    "vec2 uv = texture2D(texUV, TexCoord).ra - vec2(0.5, 0.5);"
                                    "float u = uv.r;"
                                    "float v = uv.g;"
                                    "vec3 yuv = vec3(y, %1, %2);"
                                    "vec3 rgb = mat3(1.0,      1.0,      1.0,"
                                                    "0.0,      -0.1868,  1.8556,"
                                                    "1.5748,   -0.4681,   0.0) * yuv;"
                                    "gl_FragColor = vec4(rgb, 1.0);"
                                    "}");
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
        /*片段着色器，一个输入(纹理坐标)，三个纹理采样器(在外部绑定纹理及通道)，一个输出(vec4,片段纹理颜色，低版本为内置的gl_FragColor,
         *高版本需要通过out vec4 FragColor自定义输出)，通过采样器获取对应通道的纹理数据(可以通过插值匹配计算纹理坐标区域各点的值)，然后
         *通过yuv转rgb公式，通过矩阵计算对应点的rgb数据输出。*/
        QString tmpShader = QString("varying vec2 TexCoord;"
                                    "uniform sampler2D texY;"
                                    "uniform sampler2D texU;"
                                    "uniform sampler2D texV;"
                                    ""
                                    "void main()"
                                    "{"
                                    "float y = texture2D(texY, TexCoord).r;"
                                    "float u = texture2D(texU, TexCoord).r - 0.5;"
                                    "float v = texture2D(texV, TexCoord).r - 0.5;"
                                    "vec3 yuv = vec3(y, %1, %2);"
                                    "vec3 rgb = mat3(1.0,      1.0,      1.0,"
                                                    "0.0,      -0.1868,  1.8556,"
                                                    "1.5748,   -0.4681,   0.0) * yuv;"
                                    "gl_FragColor = vec4(rgb, 1.0);"
                                    "}");
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
            //添加version头
            fragmentShader.prepend(QString("#version %1\n").arg(glslVersion));
        }
        else
        {
            //添加version头和默认精度(低版本需要明确指定精度)
            fragmentShader.prepend(QString("#version %1\nprecision highp float;").arg(glslVersion));
        }
    }

    //qDebug()<<"fragmentShader:"<<fragmentShader;

}
/*
 *@brief:  初始化纹理对象
 *注:在初始化QOpenGLTexture纹理对象时，setFormat的参数很关键，之前在PC端(OpenGL)使用R8_UNorm、RG8_UNorm、RGB8_UNorm、RGBA8_UNorm
 *设置1-4通道的格式，运行没有问题。但在arm端(OpenGL es 2.0)怎么都出不来图像，经过排查确定是纹理对象的问题，好像是因为opengl es 2.0比较特殊
 *所以专门定义了一组LuminanceFormat、LuminanceAlphaFormat、RGBFormat、RGBAFormat来适配1-4通道格式，并且实测PC端也可以兼容该格式。
 *@date:   2024.04.08
 */
void YuvRenderingWidget::initTexture()
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
        texture1.allocateStorage();
        //纹理2对应uv分量通道
        texture2.setSize(pixelWidth/2,pixelHeight);//yuv422格式，uv宽除以2，高不变
        texture2.setFormat(QOpenGLTexture::RGBAFormat);//四通道格式(每四个字节提取一组UV)
        texture2.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture2.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture2.allocateStorage();
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
        texture1.allocateStorage();
        //纹理2对应uv分量通道
        texture2.setSize(pixelWidth/2,pixelHeight/2);//yuv420格式，uv宽高均除以2
        texture2.setFormat(QOpenGLTexture::LuminanceAlphaFormat);//双通道格式(每一个点有两个数据)
        texture2.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture2.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture2.allocateStorage();
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
        texture1.allocateStorage();
        //纹理2对应u/v分量通道
        texture2.setSize(pixelWidth/2,pixelHeight/2);//yuv420格式，uv宽高均除以2
        texture2.setFormat(QOpenGLTexture::LuminanceFormat);//单通道格式
        texture2.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture2.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture2.allocateStorage();
        //纹理3对应u/v分量通道
        texture3.setSize(pixelWidth/2,pixelHeight/2);//yuv420格式，uv宽高均除以2
        texture3.setFormat(QOpenGLTexture::LuminanceFormat);//单通道格式
        texture3.setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
        texture3.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture3.allocateStorage();
    }
}
/*
 *@brief:  绘制纹理(流程：绑定纹理对象(关联着色器采样器)->绘制纹理->释放绑定）
 *该函数要在paintGL()中调用，除了有必须的绘制纹理操作，还需要不断的绑定纹理单元，否则可能出现异常
 *@date:   2024.04.08
 */
void YuvRenderingWidget::drawTexture()
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
 *@brief:  更新(渲染)yuv帧数据
 *注：此处调用QOpenGLTexture的setData时，参数PixelFormat需要与initTexture()中的format保持一致，初期使用Red、RG、RGB、RGBA，
 *后面为了与LuminanceFormat等对应起来，改用Luminance、LuminanceAlpha、RGB、RGBA。
 *@date:   2024.04.09
 *@param:  yuvFrame[planes]:yuv帧指针数组，planes根据pixelFormat格式在内部自动确定
 *对于多平面planes每一个元素对应着一个平面(不连续)，对于单平面yuvFrame[0]即完整的yuv数据
 */
void YuvRenderingWidget::updateYuvFrameSlot(uchar *yuvFrame[])
{
    //确保已经初始化opengl相关资源(initializeGL)，否则直接操作纹理会出错
    if(!this->isVisible())
    {
        return;
    }
    //one planes格式，设置两个纹理对象数据
    if(pixelFormat == V4L2_PIX_FMT_YUYV ||
            pixelFormat == V4L2_PIX_FMT_YVYU)
    {
        //纹理对象能够根据size自动读取对应字节的数据
        texture1.setData(QOpenGLTexture::LuminanceAlpha, QOpenGLTexture::UInt8, yuvFrame[0]);
        texture2.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, yuvFrame[0]);
    }
    //two planes格式，设置两个纹理对象数据
    else if(pixelFormat == V4L2_PIX_FMT_NV12 ||
            pixelFormat == V4L2_PIX_FMT_NV21)
    {
        //纹理对象能够根据size自动读取对应字节的数据
        texture1.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvFrame[0]);
        texture2.setData(QOpenGLTexture::LuminanceAlpha, QOpenGLTexture::UInt8, yuvFrame[0]+pixelWidth*pixelHeight);
    }
    //three planes格式，设置三个纹理对象数据
    else if(pixelFormat == V4L2_PIX_FMT_YUV420 ||
            pixelFormat == V4L2_PIX_FMT_YVU420)
    {
        //纹理对象能够根据size自动读取对应字节的数据
        texture1.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvFrame[0]);
        texture2.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvFrame[0]+pixelWidth*pixelHeight);
        texture3.setData(QOpenGLTexture::Luminance, QOpenGLTexture::UInt8, yuvFrame[0]+pixelWidth*pixelHeight*5/4);
    }

    update();
}
