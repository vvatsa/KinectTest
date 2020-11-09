#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>
#include <chrono>
#include <thread>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
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

const int WIDTH = 1920; // 176;
const int HEIGHT = 1080; //144;
const int FRAME_BYTES = 3 * HEIGHT * WIDTH / 2;

struct ImageSize {
    int width;
    int height;
};



ImageSize get_sensor_size(libfreenect2::Freenect2Device *dev){
    ImageSize image_size{};


}

bool setup_v4l2loopback(int vid_fd){
    struct v4l2_format vid_format{};

    std::memset(&vid_format, 0, sizeof(vid_format));
    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if(ioctl(vid_fd, VIDIOC_G_FMT, &vid_format) == -1){
        return false;
    }
    vid_format.fmt.pix.width = WIDTH;
    vid_format.fmt.pix.height = HEIGHT;
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    vid_format.fmt.pix.sizeimage = FRAME_BYTES;
    vid_format.fmt.pix.field = V4L2_FIELD_NONE;

    if(ioctl(vid_fd, VIDIOC_S_FMT, &vid_format) == -1){
        return false;
    }

    return true;
}

int main() {

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    auto _log = libfreenect2::createConsoleLogger(libfreenect2::Logger::Debug);
    _log->log(_log->Info, "Logging setup done.");
    libfreenect2::Freenect2 kinect;
    if (! kinect.enumerateDevices() > 0){
        _log->log(_log->Error, "Kinect Device not found");
        return 1;
    }

    // setup v4l2loopback device
    int fd;
    fd = open( "/dev/video0", O_RDWR);
    if ( fd == -1){
        _log->log(_log->Error, "Can't open video device for writing");
        return 1;
    }
    setup_v4l2loopback(fd);

    _log->log(_log->Info, "Video output device setup done.");

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
    dev->getColorCameraParams();
    dev->setColorFrameListener(&listener);
    std::cout << "XXX: " << dev->getSerialNumber() << std::endl;

    dev->startStreams(true, false);

    cv::Mat cv_matrix;
    cv::Mat new_mat;
    cv::Size frame_size(WIDTH, HEIGHT);
    std::string filename;
    const int frames_per_second = 30;
    const int sleep_for = 1000 / frames_per_second;
    std::vector<int> img_param;
    img_param.push_back(cv::ImwriteFlags::IMWRITE_JPEG_QUALITY);
    img_param.push_back(100);

    // record length in seconds
    int length = 6;

    int max_count = frames_per_second * length;
    int frame_count = 0;



    while (!protonect_shutdown){
        frame_count ++;
        _log->log(_log->Debug, "Waiting for frame " + std::to_string(frame_count));
        if (!listener.waitForNewFrame(frames, 2*1000)) {
            _log->log(_log->Error, "FRAME TIMEOUT");
            listener.release(frames);
            break;
        } else {
            _log->log(_log->Info, "Got Frame :) " + std::to_string(frame_count));
        }

        libfreenect2::Frame *rgb = frames[libfreenect2::Frame::Color];

        filename = "/home/wish/frames/frame_" + std::to_string(frame_count) + ".jpg";

        cv_matrix = cv::Mat(rgb->height, rgb->width, CV_8UC4, rgb->data);
        cv::cvtColor(cv_matrix, cv_matrix, cv::COLOR_BGR2YUV_I420);
//        cv::resize(cv_matrix, new_mat, frame_size);
        //cv::imwrite(filename, cv_matrix, img_param);
        _log->log(_log->Info, "Sending frame.... ");
        write(fd, cv_matrix.data, FRAME_BYTES);
        listener.release(frames);
        _log->log(_log->Debug, "Sleep");
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for));
    }

    dev->stop();
    dev->close();
    return 0;
}

