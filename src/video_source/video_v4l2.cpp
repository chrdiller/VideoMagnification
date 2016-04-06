#include <video_source.h>
#include <helpers/v4l2.hpp>

bool VideoSource::open(const std::string video_filename) {
    bool open_success = true;
    is_live_feed = false;
    first_playback = true;
    try { video_source.open(video_filename); }
    catch(...) { open_success = false; }
    return (open_success && video_source.isOpened());
}

bool VideoSource::open(const int video_device) {
    is_live_feed = true;
    first_playback = true;
    return video_source_v4l2.open("/dev/video"+std::to_string(video_device));
}

void VideoSource::release() {
    if(is_live_feed) {
        video_source_v4l2.release();
        video_source_v4l2 = V4L2Capture();
    }
    else {
        video_source.release();
        video_source = cv::VideoCapture();
    }
}

VideoSource::~VideoSource() {
    if(is_live_feed)
        video_source_v4l2.release();
    else
        video_source.release();
}

void VideoSource::operator>>(cv::Mat& out) noexcept {
    if(is_live_feed) {
        video_source_v4l2 >> out;
    } else {
        video_source >> out;
        if(out.rows == 0) { //Loop the video
            first_playback = false;
            video_source.set(CV_CAP_PROP_POS_FRAMES, 0.0);
            video_source >> out;
        }
    }
}

int VideoSource::get_fps() const noexcept {
    if(is_live_feed)
        return video_source_v4l2.get_fps();
    else
        return static_cast<int>(video_source.get(CV_CAP_PROP_FPS));
}

int VideoSource::get_n_frames() const noexcept {
    if(is_live_feed) return 0;
    else return static_cast<int>(video_source.get(CV_CAP_PROP_FRAME_COUNT));
}

std::vector<v4l2_option> VideoSource::list_options() {
    return video_source_v4l2.list_options();
}

int VideoSource::set_option_value(const v4l2_option& option, const int new_value) {
    return video_source_v4l2.set_option_value(option, new_value);
}

const cv::Size VideoSource::get_frame_size() {
    if(is_live_feed)
        return video_source_v4l2.get_frame_size();
    else
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

