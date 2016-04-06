/* VideoMagnification - Magnify motions and detect heartbeats
Copyright (C) 2016 Christian Diller

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>. */

#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <opencv2/videoio.hpp>
#include <helpers/common.h>

//This will be set by CMake
//#define V4L2_CAPTURE //Define this here for development to make CLion happy
#ifdef V4L2_CAPTURE
#include <helpers/v4l2.hpp>
#endif

class VideoSource {
public:
    VideoSource() {};
    VideoSource(const VideoSource&) = delete;

    ~VideoSource();

    bool open(const std::string video_filename);
    bool open(const int video_device);

    void release();

    void operator>>(cv::Mat& out) noexcept;
    int get_fps() const noexcept;
    int get_n_frames() const noexcept;

    const cv::Size get_frame_size();
    void start_from_beginning();
    const bool is_first_playback();

    std::vector<v4l2_option> list_options();
    int set_option_value(const v4l2_option& option, const int new_value);

private:
    cv::VideoCapture video_source;
    bool first_playback;
    bool is_live_feed;
#ifdef V4L2_CAPTURE
    V4L2Capture video_source_v4l2;
#endif
};

#endif //VIDEO_SOURCE_H
