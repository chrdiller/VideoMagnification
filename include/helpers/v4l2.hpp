#ifndef V4L2_HPP
#define V4L2_HPP

#include <string>
#include <opencv2/core.hpp>

#include <fcntl.h>
#include <libv4l2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include <helpers/common.h>

#include <tbb/tick_count.h>

using std::string;

inline bool failed(std::string why) {
    std::cerr << why << std::endl;
    return false;
}

class V4L2Capture {
public:
    V4L2Capture() { }

    ~V4L2Capture() {
        release();
    }

    bool open(const std::string device, cv::Size_<unsigned int> _frame_size = {640, 480}) {
        frame_size = _frame_size;
        //Open the specified device
        if((video_device_fd = v4l2_open(device.c_str(), O_RDWR)) < 0)
            return failed("Could not open video device " + device);

        //Query capabilities of that device
        v4l2_capability capabilities = {0};
        if(v4l2_ioctl(video_device_fd, VIDIOC_QUERYCAP, &capabilities) < 0)
            return failed("Could not query capabilities of video device " + device);

        //Print some device information
        string driver_name = string(capabilities.driver, capabilities.driver+sizeof(capabilities.driver));
        driver_name = driver_name.substr(0, driver_name.find(static_cast<char>(0)));
        string device_name = string(capabilities.card, capabilities.card+sizeof(capabilities.card));
        device_name = device_name.substr(0, device_name.find(static_cast<char>(0)));
        string bus_info = string(capabilities.bus_info, capabilities.bus_info+sizeof(capabilities.bus_info));
        bus_info = bus_info.substr(0, bus_info.find(static_cast<char>(0)));
        std::cout << "Info for video device " << device << ": Driver name: " << driver_name <<
             ", Device name: " << device_name << ", Bus identifier: " << bus_info << std::endl;

        //Check if video streaming is supported
        if(!(capabilities.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
           !(capabilities.capabilities & V4L2_CAP_STREAMING))
            return failed("Video device " + device + " does not support video streaming");

        //Set the format
        v4l2_format format = {0};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        format.fmt.pix.width = frame_size.width;
        format.fmt.pix.height = frame_size.height;

        if(v4l2_ioctl(video_device_fd, VIDIOC_S_FMT, &format) < 0)
            return failed("Could not set the video format of video device " + device);

        v4l2_streamparm streamparm = {0};
        streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(video_device_fd, VIDIOC_G_PARM, &streamparm) < 0)
            return failed("Could not query streamparm");

        streamparm.parm.capture.capturemode |= V4L2_CAP_TIMEPERFRAME;
        streamparm.parm.capture.timeperframe.numerator = 1;
        streamparm.parm.capture.timeperframe.denominator = 33;
        if(v4l2_ioctl(video_device_fd, VIDIOC_S_PARM, &streamparm) < 0)
            return failed("Could not set streamparm");

        //Query all available options
        v4l2_queryctrl queryctrl = {0};
        queryctrl.id = V4L2_CTRL_CLASS_USER | V4L2_CTRL_FLAG_NEXT_CTRL;
        while (v4l2_ioctl(video_device_fd, VIDIOC_QUERYCTRL, &queryctrl) == 0) {
            if (V4L2_CTRL_ID2CLASS(queryctrl.id) != V4L2_CTRL_CLASS_USER) break;
            if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;

            v4l2_option current_option = {0};
            current_option.id = queryctrl.id;
            if(queryctrl.type == 1) current_option.type = v4l2_option_type::INTEGER;
            else if(queryctrl.type == 2) current_option.type = v4l2_option_type::BOOLEAN;
            else if(queryctrl.type == 3) current_option.type = v4l2_option_type::MENU;
            else continue;
            current_option.name = std::string(queryctrl.name, queryctrl.name+sizeof(queryctrl.name));
            current_option.name = current_option.name.substr(0, current_option.name.find(static_cast<char>(0)));
            current_option.minimum = queryctrl.minimum;
            current_option.maximum = queryctrl.maximum;
            current_option.current = queryctrl.default_value;
            capture_options.push_back(current_option);

            queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
        if (errno != EINVAL)
            return failed("Querying controls failed");

        //Request buffer requirements
        v4l2_requestbuffers bufrequest = {0};
        bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufrequest.memory = V4L2_MEMORY_MMAP;
        bufrequest.count = 1;

        if(v4l2_ioctl(video_device_fd, VIDIOC_REQBUFS, &bufrequest) < 0)
            return failed("Requesting buffer failed");

        bufferinfo = {0};
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index = 0;

        if(v4l2_ioctl(video_device_fd, VIDIOC_QUERYBUF, &bufferinfo) < 0)
            return failed("Querying buffer failed");

        //Create the frame buffer
        buffer = static_cast<unsigned char*>(mmap(NULL, bufferinfo.length,
                PROT_READ | PROT_WRITE, MAP_SHARED,
                video_device_fd, bufferinfo.m.offset
        ));

        if(buffer == MAP_FAILED)
            return failed("Memory mapping failed");

        //Prepare for buffering
        bufferinfo = {0};
        bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufferinfo.memory = V4L2_MEMORY_MMAP;
        bufferinfo.index = 0;

        if(v4l2_ioctl(video_device_fd, VIDIOC_STREAMON, &bufferinfo.type) < 0)
            return failed("Activating the stream failed");

        if(v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &bufferinfo) < 0)
            return failed("Queuing a new buffer failed");

        return true; //Everything went well
    }

    void operator>>(cv::Mat& out) {
        if(v4l2_ioctl(video_device_fd, VIDIOC_DQBUF, &bufferinfo) < 0)
            failed("Grabbing the current output frame failed");

        if(v4l2_ioctl(video_device_fd, VIDIOC_QBUF, &bufferinfo) < 0)
            failed("Queuing a new buffer failed");

        //For uncompressed frames
        /*cv::Mat output(frame_size.height, frame_size.width, CV_8UC2, buffer);
        cv::cvtColor(output, out, CV_YUV2BGR_YVYU);*/

        out = cv::imdecode(std::vector<uchar>(buffer, buffer+bufferinfo.length), CV_LOAD_IMAGE_COLOR);
        if(out.empty())
            out = cv::Mat(100,100,CV_8UC3);
    }

    void release() {
        if(v4l2_ioctl(video_device_fd, VIDIOC_STREAMOFF, &bufferinfo.type) < 0)
            failed("Deactivating the stream failed");

        close(video_device_fd);
    }

    std::vector<v4l2_option> list_options() {
        return capture_options;
    }

    int set_option_value(const v4l2_option& option, const int new_value) {
        v4l2_control control = {0};
        control.id = option.id;
        control.value = new_value;

        if (-1 == v4l2_ioctl(video_device_fd, VIDIOC_S_CTRL, &control)) {
            failed("Could not set the value; errno=" + std::to_string(errno));
            return option.current;
        }
        else
            return new_value;
    }

    int get_fps() const {
        v4l2_streamparm streamparm = {0};
        streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (v4l2_ioctl(video_device_fd, VIDIOC_G_PARM, &streamparm) < 0)
            return failed("Could not query streamparm");

        return 1000 * streamparm.parm.capture.timeperframe.numerator / streamparm.parm.capture.timeperframe.denominator;
    }

    cv::Size get_frame_size() const {
        return frame_size;
    }


private:
    int video_device_fd;

    v4l2_buffer bufferinfo;
    unsigned char* buffer = nullptr;

    std::vector<v4l2_option> capture_options;

    cv::Size_<unsigned int> frame_size;
};

#endif //V4L2_HPP
