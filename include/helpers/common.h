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

#ifndef COMMON_H
#define COMMON_H

#include <opencv2/opencv.hpp>

enum class v4l2_option_type {
    INTEGER, BOOLEAN, MENU
};

struct v4l2_option {
    int id;
    std::string name;
    v4l2_option_type type;
    int minimum;
    int maximum;
    int current;
};

struct mouse_selection {
    bool selecting = false;
    bool complete = false;
    bool fresh = false;
    cv::Point2i point_from;
    cv::Point2i point_to;

    cv::Rect rect() {
        int x1, x2, y1, y2;
        x1 = std::min(point_from.x, point_to.x);
        x2 = std::max(point_from.x, point_to.x);
        y1 = std::min(point_from.y, point_to.y);
        y2 = std::max(point_from.y, point_to.y);
        return cv::Rect(x1, y1, x2-x1, y2-y1);
    }
};

struct analysis_data {
    std::vector<double> timedomain_keys;
    std::vector<double> timedomain_values;
    std::vector<double> frequencydomain_keys;
    std::vector<double> frequencydomain_values;
    double heartbeat_number;
};

enum class spatial_filter_type {
    NONE, LAPLACIAN, GAUSSIAN
};

enum class temporal_filter_type {
    IDEAL, IIR
};

struct parameter_store {
    //Filter types
    spatial_filter_type spatial_filter;
    temporal_filter_type temporal_filter;

    //Color
    int color_convert_forward, color_convert_backward;
    std::vector<bool> active_channels;

    //Spatial filter parameters
    cv::Rect roi_rect;
    int n_buffered_frames;
    int n_layers;

    //Temporal filter parameters
    float alpha;
    float lambda_c;
    float min_freq;
    float max_freq;
    float cutoffLo;
    float cutoffHi;

    //Video output parameters
    bool write_to_file;
    bool convert_whole_video;
    int output_fourcc;
    std::string video_output_filename;

    //Misc
    int fps;
    int n_channels;
    bool analyze_heartbeat;
    bool shutdown;
};

inline cv::Size fit_to_layer(const cv::Size& original, const int layer_id) noexcept {
    return cv::Size(original.width/static_cast<int>(pow(2L, static_cast<long>(layer_id))),
                    original.height/static_cast<int>(pow(2L, static_cast<long>(layer_id))));
}

inline cv::Rect align_rect(const cv::Rect& original_rect, const int n_layers) noexcept {
    const int pixel_align = static_cast<int>(pow(2L, n_layers-1));
    const int overlapping_pixels_width = original_rect.width % pixel_align;
    const int overlapping_pixels_height = original_rect.height % pixel_align;
    return cv::Rect(original_rect.x+overlapping_pixels_width/2,
                    original_rect.y+overlapping_pixels_height/2,
                    original_rect.width-overlapping_pixels_width,
                    original_rect.height-overlapping_pixels_height);
}

#endif //COMMON_H
