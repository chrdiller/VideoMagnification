#include <video_source.h>

#include <thread>
#include <helpers/common.h>
#include <iostream>

bool VideoSource::open(const std::string video_filename) {
    bool open_success = true;
    is_live_feed = false;
    try { video_source.open(video_filename); }
    catch(...) { open_success = false; }
    first_playback = true;
    return (open_success && video_source.isOpened());
}

bool VideoSource::open(const int video_device) {
    bool open_success = true;
    is_live_feed = true;
    try { video_source.open(video_device); }
    catch(...) { open_success = false; }
    first_playback = true;
    return (open_success && video_source.isOpened());
}

void VideoSource::release() {
    video_source.release();
    video_source = cv::VideoCapture();
}

VideoSource::~VideoSource() {
    video_source.release();
}

int VideoSource::get_fps() const noexcept {
    return static_cast<int>(video_source.get(CV_CAP_PROP_FPS));
}

int VideoSource::get_n_frames() const noexcept {
    return static_cast<int>(video_source.get(CV_CAP_PROP_FRAME_COUNT));
}

void VideoSource::operator>>(cv::Mat& out) noexcept {
    video_source >> out;
    if(out.rows == 0) { //Loop the video
        first_playback = false;
        video_source.set(CV_CAP_PROP_POS_FRAMES, 0.0);
        video_source >> out;
    }
}

const cv::Size VideoSource::get_frame_size() {
    return cv::Size(static_cast<int>(video_source.get(cv::CAP_PROP_FRAME_WIDTH)),
                    static_cast<int>(video_source.get(cv::CAP_PROP_FRAME_HEIGHT)));
}

void VideoSource::start_from_beginning() {
    if(!is_live_feed) {
        video_source.set(cv::CAP_PROP_POS_FRAMES, 0);
        first_playback = true;
    }
}

const bool VideoSource::is_first_playback() {
    return first_playback;
}

std::vector<v4l2_option> VideoSource::list_options() {
    return std::vector<v4l2_option>(0);
}

int VideoSource::set_option_value(const v4l2_option& option, const int new_value) {
    //Nothing to do here ...
    return 0;
}
