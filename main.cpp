#include <unistd.h>
#include <signal.h>
#include <thread>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
/// [headers]
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/logger.h>
#include <opencv2/opencv.hpp>

/// [headers]


bool protonect_shutdown = false; ///< Whether the running application should shut down.

void sig_handler(int s)
{
    protonect_shutdown = true;
}


libfreenect2::Logger * logger;

void debug(const std::string& message){
    logger->log(logger->Debug, message);
}

void info(const std::string& message){
    logger->log(logger->Info, message);
}

void error(const std::string& message){
    logger->log(logger->Error, message);
}

struct ImageSize {
    const int rgb_width = 1920;
    const int rgb_height = 1080;
    const int rgb_frame_size = 3 * rgb_height * rgb_width / 2;
    const int depth_width = 512;
    const int depth_height = 424;
    const int pixel_byte = 4;
} image_size;


bool setup_v4l2loopback(int vid_fd){
    struct v4l2_format vid_format{};

    std::memset(&vid_format, 0, sizeof(vid_format));
    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if(ioctl(vid_fd, VIDIOC_G_FMT, &vid_format) == -1){
        error("IOCTL can't read from from video dev fd.");
        return false;
    }
    vid_format.fmt.pix.width = image_size.rgb_width;
    vid_format.fmt.pix.height = image_size.rgb_height;
    vid_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    vid_format.fmt.pix.sizeimage = image_size.rgb_frame_size;
    vid_format.fmt.pix.field = V4L2_FIELD_NONE;

    if(ioctl(vid_fd, VIDIOC_S_FMT, &vid_format) == -1){
        error("IOCTL can't write to video dev fd.");
        return false;
    }

    return true;
}

int main() {

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    logger = libfreenect2::createConsoleLogger(libfreenect2::Logger::Info);
    info("Logging setup done.");

    libfreenect2::Freenect2 kinect;
    if (! kinect.enumerateDevices() > 0){
        error("Kinect Device not found");
        return 1;
    }

    // setup v4l2loopback device
    int fd;
    fd = open( "/dev/video0", O_RDWR);
    if ( fd == -1){
        error("Can't open video device for writing");
        return 1;
    }
    if (!setup_v4l2loopback(fd)){
        error("Can't setup videoloop back device");
        return 1;
    }

    info("Video output device setup done.");


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

    cv::Mat cv_matrix;
    cv::Mat new_mat;
    std::string filename;
    std::vector<int> img_param;
    img_param.push_back(cv::ImwriteFlags::IMWRITE_JPEG_QUALITY);
    img_param.push_back(100);
    int frame_count = 0;

    dev->startStreams(true, false);

    info("Running, <ctrl-c> to exit.");
    while (!protonect_shutdown){
        frame_count ++;
        debug("Waiting for frame " + std::to_string(frame_count));
        if (!listener.waitForNewFrame(frames, 2*1000)) {
            error("FRAME TIMEOUT");
            listener.release(frames);
            break;
        } else {
            debug("Got Frame :) " + std::to_string(frame_count));
        }

        libfreenect2::Frame *rgb = frames[libfreenect2::Frame::Color];
        cv_matrix = cv::Mat(rgb->height, rgb->width, CV_8UC4, rgb->data);
        cv::flip(cv_matrix, cv_matrix, 1);
        // Loopback pixel format V4L2_PIX_FMT_YUV420 matches COLOR_BGR2YUV_I420
        cv::cvtColor(cv_matrix, cv_matrix, cv::COLOR_BGR2YUV_I420);

        debug("Sending frame.... ");
        write(fd, cv_matrix.data, image_size.rgb_frame_size);
        listener.release(frames);
    }

    dev->stop();
    dev->close();
    return 0;
}
