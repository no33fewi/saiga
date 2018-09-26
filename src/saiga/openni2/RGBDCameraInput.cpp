/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "RGBDCameraInput.h"
#include "saiga/image/imageTransformations.h"
#include "saiga/util/threadName.h"

#include <OpenNI.h>
#include <thread>
#include "internal/noGraphicsAPI.h"


namespace Saiga {

#define CHECK_NI(_X) \
{\
    auto ret = _X; \
    if(ret != openni::STATUS_OK) \
{ \
    cout << "Openni error in " << #_X << endl \
    << "code: " << ret << endl \
    << "message: " << openni::OpenNI::getExtendedError() << endl;\
    SAIGA_ASSERT(0); \
}\
}



RGBDCameraInput::RGBDCameraInput(RGBDCameraInput::CameraOptions rgbo, RGBDCameraInput::CameraOptions deptho, float depthFactor)
    : RGBDCamera(rgbo,deptho),  frameBuffer(10), depthFactor(depthFactor)
{
    CHECK_NI(openni::OpenNI::initialize());
    eventThread = std::thread(&RGBDCameraInput::eventLoop,this);
}

RGBDCameraInput::~RGBDCameraInput()
{
    cout << "~RGBDCameraInput" << endl;
    running = false;
    eventThread.join();

//    device.reset();
//    openni::OpenNI::shutdown();
}

std::shared_ptr<RGBDCamera::FrameData> RGBDCameraInput::waitForImage()
{
     return frameBuffer.get();
}

std::shared_ptr<RGBDCamera::FrameData> RGBDCameraInput::tryGetImage()
{
    std::shared_ptr<FrameData> img;
    frameBuffer.tryGet(img);
    return img;
}

bool RGBDCameraInput::isOpened()
{
    return foundCamera;
}

bool RGBDCameraInput::open()
{
    resetCamera();

    openni::Status rc = openni::STATUS_OK;


    const char* deviceURI = openni::ANY_DEVICE;



    rc = device->open(deviceURI);
    if (rc != openni::STATUS_OK)
    {
        return false;
    }

    CHECK_NI(depth->create(*device, openni::SENSOR_DEPTH));
    CHECK_NI(color->create(*device, openni::SENSOR_COLOR));
    SAIGA_ASSERT(depth->isValid() && color->isValid());

    {
        CameraOptions co =  deptho ;
        const openni::Array<openni::VideoMode>& modes = depth->getSensorInfo().getSupportedVideoModes();
        int found = -1;
        for(int i = 0; i < modes.getSize(); ++i)
        {
            const openni::VideoMode& mode = modes[i];

            if(mode.getResolutionX() == co.w &&
                    mode.getResolutionY() == co.h &&
                    mode.getFps() == co.fps &&
                    mode.getPixelFormat() == openni::PIXEL_FORMAT_DEPTH_1_MM
                    )
            {
                found = i;
                break;
            }
        }
        SAIGA_ASSERT(found != -1);
        CHECK_NI(depth->setVideoMode(modes[found]));
    }

    {
        CameraOptions co =  rgbo ;
        const openni::Array<openni::VideoMode>& modes = color->getSensorInfo().getSupportedVideoModes();
        int found = -1;
        for(int i = 0; i < modes.getSize(); ++i)
        {
            const openni::VideoMode& mode = modes[i];

            if(mode.getResolutionX() == co.w &&
                    mode.getResolutionY() == co.h &&
                    mode.getFps() == co.fps &&
                    mode.getPixelFormat() == openni::PIXEL_FORMAT_RGB888
                    )
            {
                found = i;
                break;
            }
        }
        SAIGA_ASSERT(found != -1);
        CHECK_NI(color->setVideoMode(modes[found]));
    }


    CHECK_NI(color->start());
    CHECK_NI(depth->start());


    cout << "RGBD Camera opened."  << endl;

    return true;
}

void RGBDCameraInput::resetCamera()
{
    m_depthFrame = std::make_shared<openni::VideoFrameRef>();
    m_colorFrame = std::make_shared<openni::VideoFrameRef>();

    depth = std::make_shared<openni::VideoStream>();
    color = std::make_shared<openni::VideoStream>();

    device = std::make_shared<openni::Device>();
}

bool RGBDCameraInput::waitFrame(FrameData &data)
{
    openni::VideoStream* streams[2] = {depth.get(),color.get()};
    int streamIndex;

    auto wret = openni::OpenNI::waitForAnyStream(streams,2,&streamIndex,1000);
    if(wret != openni::STATUS_OK) return false;

    setNextFrame(data);


    bool ret = true;
    if(streamIndex == 0)
    {
        ret &= readDepth(data.depthImg);

        wret = openni::OpenNI::waitForAnyStream(streams+1,1,&streamIndex,1000);
        if(wret != openni::STATUS_OK) return false;

        ret &= readColor(data.colorImg);
    }else{
        ret &= readColor(data.colorImg);

        wret = openni::OpenNI::waitForAnyStream(streams,1,&streamIndex,1000);
        if(wret != openni::STATUS_OK) return false;

        ret &= readDepth(data.depthImg);
    }


    return ret;
}

bool RGBDCameraInput::readDepth(DepthImageType::ViewType depthImg)
{
    auto res = depth->readFrame(m_depthFrame.get());
    if (res != openni::STATUS_OK) return false;

    SAIGA_ASSERT(m_depthFrame->getWidth() == depthImg.w &&
                 m_depthFrame->getHeight() == depthImg.h);

    ImageView<uint16_t> rawDepthImg(
                m_depthFrame->getHeight(),
                m_depthFrame->getWidth(),
                m_depthFrame->getStrideInBytes(),
                (void*)m_depthFrame->getData());

    for(int i = 0; i < rawDepthImg.height; ++i)
    {
        for(int j =0; j < rawDepthImg.width; ++j)
        {
            depthImg(i,j) = rawDepthImg(i,rawDepthImg.width-j-1) * depthFactor;
        }
    }
    return true;
}

bool RGBDCameraInput::readColor(RGBImageType::ViewType colorImg)
{

    auto res = color->readFrame(m_colorFrame.get());
    if (res != openni::STATUS_OK) return false;

    SAIGA_ASSERT(m_colorFrame->getWidth() == colorImg.w &&
                 m_colorFrame->getHeight() == colorImg.h);

    ImageView<ucvec3> rawImg(
                m_colorFrame->getHeight(),
                m_colorFrame->getWidth(),
                m_colorFrame->getStrideInBytes(),
                (void*)m_colorFrame->getData());

    for(int i = 0; i < rawImg.height; ++i)
    {
        for(int j =0; j < rawImg.width; ++j)
        {
            colorImg(i,j) = ucvec4(rawImg(i,rawImg.width-j-1),255);
        }
    }

    return true;
}

void RGBDCameraInput::eventLoop()
{
    running = true;

    setThreadName("Saiga::NI");

    std::shared_ptr<FrameData> tmp = makeFrameData();


    while(running)
    {

        if(!foundCamera)
        {
            foundCamera  = open();

            if(!foundCamera)
            {

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        if(!waitFrame(*tmp))
        {
            cout << "lost camera connection!" << endl;
            foundCamera = false;
            continue;
        }

//        if(!frameBuffer.tryAdd(tmp))
        frameBuffer.addOverride(tmp);
        {
//            cout << "buffer full" << endl;
        }
        tmp = makeFrameData();

    }
}


}
