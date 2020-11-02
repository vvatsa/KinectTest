#include <iostream>
#include <cstdlib>
#include <signal.h>
#include <chrono>
#include <thread>

/// [headers]
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/logger.h>
#include <opencv2/opencv.hpp>
/// [headers]

bool protonect_shutdown = false; ///< Whether the running application should shut down.

void sigint_handler(int s)
{
    protonect_shutdown = true;
}

bool protonect_paused = false;
libfreenect2::Freenect2Device *devtopause;

//Doing non-trivial things in signal handler is bad. If you want to pause,
//do it in another thread.
//Though libusb operations are generally thread safe, I cannot guarantee
//everything above is thread safe when calling start()/stop() while
//waitForNewFrame().
void sigusr1_handler(int s)
{
    if (devtopause == 0)
        return;
/// [pause]
    if (protonect_paused)
        devtopause->start();
    else
        devtopause->stop();
    protonect_paused = !protonect_paused;
/// [pause]
}



int main() {
    auto _log = libfreenect2::createConsoleLogger(libfreenect2::Logger::Debug);
    _log->log(_log->Info, "Logging setup done.");
    libfreenect2::Freenect2 kinect;
    if (! kinect.enumerateDevices() > 0){
        _log->log(_log->Error, "Kinect Device not found");
        return 1;
    }

    std::cout << kinect.getDefaultDeviceSerialNumber() << std::endl;

    libfreenect2::Freenect2Device *dev;
    libfreenect2::PacketPipeline *pipeline;

    // setup frame listener, just rgb for now.
    int frame_types = 0;
    frame_types |= libfreenect2::Frame::Color;
    libfreenect2::SyncMultiFrameListener listener(frame_types);
    libfreenect2::FrameMap frames;

    pipeline = new libfreenect2::CpuPacketPipeline();
    auto serial_number = kinect.getDefaultDeviceSerialNumber();
    dev = kinect.openDevice(serial_number, pipeline);
    dev->setColorFrameListener(&listener);
    std::cout << "XXX: " << dev->getSerialNumber() << std::endl;

    dev->startStreams(true, false);

    cv::Mat cv_matrix;
    std::string filename;
    const int frames_per_second = 30;
    const int sleep_for = 1000 / frames_per_second;
    std::vector<int> img_param;
    img_param.push_back(cv::ImwriteFlags::IMWRITE_JPEG_QUALITY);
    img_param.push_back(100);
    int max_count = 15;
    int frame_count = 0;

    while (max_count > frame_count){
        frame_count ++;
        if (!listener.waitForNewFrame(frames, 10*1000)) {
            _log->log(_log->Error, "FRAME TIMEOUT");
            break;
        } else {
            _log->log(_log->Info, "Got Frame :) " + std::to_string(frame_count));
        }

        libfreenect2::Frame *rgb = frames[libfreenect2::Frame::Color];

        filename = "/home/wish/frames/frame_" + std::to_string(frame_count) + ".jpg";

        cv_matrix = cv::Mat(rgb->height, rgb->width, CV_8UC4, rgb->data);
        cv::imwrite(filename, cv_matrix, img_param);

        listener.release(frames);
        _log->log(_log->Debug, "Sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for));
    }

    dev->stop();
    dev->close();
    return 0;
}

