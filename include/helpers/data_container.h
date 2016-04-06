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

#ifndef DATA_CONTAINER_H
#define DATA_CONTAINER_H

#include <vector>

#include <opencv2/core.hpp>

#include <helpers/common.h>

class DataContainer {
public:
    DataContainer(parameter_store& _params) noexcept;

    //Whole frame input and output
    void push_frame(const cv::Mat_<cv::Vec3f>& frame, parameter_store& _params);
    cv::Mat_<cv::Vec3f> pop_frame() noexcept;

    //Only the frame data within the current ROI
    const cv::Mat_<cv::Vec3f> get_frame_roi();

    //Single layer input and output
    void put_layer(const int layer_id, const cv::Mat_<cv::Vec3f>& layer);
    void insert_reconstructed_layer_roi(const cv::Mat_<cv::Vec3f>& roi);
    cv::Mat_<cv::Vec3f> get_layer(const int layer_id) noexcept;

    //Access to data buffers
    cv::Mat_<float> get_input_timeseries(const int layer_id, const int timeseries_id, const int channel_id);
    cv::Mat_<float> get_output_timeseries(const int layer_id, const int timeseries_id, const int channel_id);
    cv::Mat_<float> get_fftwf_data(const int layer_id, const int timeseries_id, const int channel_id);

    //Access to iir data
    cv::Mat_<cv::Vec3f> get_lowpassLo(const int layer_id);
    cv::Mat_<cv::Vec3f> get_lowpassHi(const int layer_id);

    //Analysis data
    cv::Mat_<float> get_average_roi_pixels();

    const int get_n_used_frames();

private:
    void init_buffers();

    //Spatial data storage
    cv::Mat_<cv::Vec3f> previous_input_frame;
    cv::Mat_<cv::Vec3f> current_input_frame;

    //Temporal data storage for IIR filtering
    std::vector<cv::Mat_<cv::Vec3f>> current_layers;
    std::vector<cv::Mat_<cv::Vec3f>> lowpassLo;
    std::vector<cv::Mat_<cv::Vec3f>> lowpassHi;

    //Temporal data storage for ideal filtering
    std::vector<cv::Mat_<float>> original_temporal_buffer;
    std::vector<cv::Mat_<float>> processed_temporal_buffer;
    std::vector<cv::Mat_<float>> fftwf_buffer;

    cv::Mat_<float> average_roi_pixels;
    cv::Mat_<float> average_fft_bins;

    //Used to determine the current position within the ring buffer
    int current_frame_id = 0;

    //A copy of the current parameters
    parameter_store params;
};


#endif //DATA_CONTAINER_H
