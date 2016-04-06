#include <helpers/data_container.h>
#include <iostream>

DataContainer::DataContainer(parameter_store& _params) noexcept : params(_params) {
    init_buffers();
}

void DataContainer::push_frame(const cv::Mat_<cv::Vec3f>& frame, parameter_store& _params) {
    current_input_frame = frame;
    if(params.n_layers != _params.n_layers || params.n_buffered_frames != _params.n_buffered_frames ||
            params.roi_rect.width != _params.roi_rect.width || params.roi_rect.height != _params.roi_rect.height ||
            params.spatial_filter != _params.spatial_filter || params.temporal_filter != _params.temporal_filter) {
        params = _params;
        if(params.spatial_filter != spatial_filter_type::NONE)
            init_buffers();
    }
    params.analyze_heartbeat = _params.analyze_heartbeat;
}

cv::Mat_<cv::Vec3f> DataContainer::pop_frame() noexcept {
    previous_input_frame = current_input_frame;
    if(params.analyze_heartbeat) {
        cv::Scalar_<float> mean_value = cv::mean(current_input_frame(params.roi_rect));
        for(int i = 0; i < params.n_channels; ++i)
            average_roi_pixels.at<float>(i, current_frame_id % params.n_buffered_frames) = mean_value[i];
    }
    ++current_frame_id;
    return current_input_frame;
}


//Only the frame data within the current ROI
const cv::Mat_<cv::Vec3f> DataContainer::get_frame_roi() {
    cv::Mat roi_rect_data = current_input_frame(params.roi_rect).clone();
    current_input_frame(params.roi_rect).setTo(0);
    return roi_rect_data;
}


//Single layer input and output
void DataContainer::put_layer(const int layer_id, const cv::Mat_<cv::Vec3f>& layer) {
    if(params.temporal_filter == temporal_filter_type::IDEAL) {
        if(params.spatial_filter == spatial_filter_type::LAPLACIAN)
            layer.reshape(1, static_cast<int>(layer.total()) * params.n_channels)
                    .copyTo(original_temporal_buffer[layer_id].col(current_frame_id % params.n_buffered_frames));
        else if(params.spatial_filter == spatial_filter_type::GAUSSIAN) {
            if(layer_id < params.n_layers-1)
                current_layers[layer_id] = layer;
            else
                layer.reshape(1, static_cast<int>(layer.total()) * params.n_channels)
                        .copyTo(original_temporal_buffer[0].col(current_frame_id % params.n_buffered_frames));
        }
    }
    else
        current_layers[layer_id] = layer;
}

void DataContainer::insert_reconstructed_layer_roi(const cv::Mat_<cv::Vec3f>& roi) {
    current_input_frame(params.roi_rect) += roi;
}

cv::Mat_<cv::Vec3f> DataContainer::get_layer(const int layer_id) noexcept {
    if(params.temporal_filter == temporal_filter_type::IDEAL) {
        if(params.spatial_filter == spatial_filter_type::LAPLACIAN)
            return processed_temporal_buffer[layer_id].col(current_frame_id % params.n_buffered_frames)
                    .clone().reshape(params.n_channels, fit_to_layer(params.roi_rect.size(), layer_id).height);
        else if(params.spatial_filter == spatial_filter_type::GAUSSIAN) {
            if(layer_id < params.n_layers-1)
               return current_layers[layer_id];
            else
                return processed_temporal_buffer[0].col(current_frame_id % params.n_buffered_frames)
                        .clone().reshape(params.n_channels, fit_to_layer(params.roi_rect.size(), layer_id).height);
        }
    }
    else
        return current_layers[layer_id];
}


cv::Mat_<float> DataContainer::get_input_timeseries(const int layer_id, const int timeseries_id, const int channel_id) {
    return original_temporal_buffer[layer_id].row(timeseries_id*params.n_channels+channel_id);
}

cv::Mat_<float> DataContainer::get_output_timeseries(const int layer_id, const int timeseries_id, const int channel_id) {
    return processed_temporal_buffer[layer_id].row(timeseries_id*params.n_channels+channel_id);
}

cv::Mat_<float> DataContainer::get_fftwf_data(const int layer_id, const int timeseries_id, const int channel_id) {
    return fftwf_buffer[layer_id].row(timeseries_id*params.n_channels+channel_id);
}

cv::Mat_<cv::Vec3f> DataContainer::get_lowpassLo(const int layer_id) {
    return lowpassLo[layer_id];
}

cv::Mat_<cv::Vec3f> DataContainer::get_lowpassHi(const int layer_id) {
    return lowpassHi[layer_id];
}

cv::Mat_<float> DataContainer::get_average_roi_pixels() {
    return average_roi_pixels;
}

const int DataContainer::get_n_used_frames() {
    return current_frame_id+1;
}

void DataContainer::init_buffers() {
    current_frame_id = 0;
    if(params.temporal_filter == temporal_filter_type::IDEAL) {
        if(params.spatial_filter == spatial_filter_type::LAPLACIAN) {
            original_temporal_buffer = std::vector<cv::Mat_<float>>(params.n_layers);
            processed_temporal_buffer = std::vector<cv::Mat_<float>>(params.n_layers);
            fftwf_buffer = std::vector<cv::Mat_<float>>(params.n_layers);
            for (int layer_id = 0; layer_id < params.n_layers; ++layer_id) {
                original_temporal_buffer[layer_id] = cv::Mat_<float>::zeros(
                        fit_to_layer(params.roi_rect.size(), layer_id).area() * params.n_channels,
                        params.n_buffered_frames);
                processed_temporal_buffer[layer_id] = cv::Mat_<float>(
                        fit_to_layer(params.roi_rect.size(), layer_id).area() * params.n_channels,
                        params.n_buffered_frames);
                fftwf_buffer[layer_id] = cv::Mat_<float>(
                        fit_to_layer(params.roi_rect.size(), layer_id).area() * params.n_channels,
                        params.n_buffered_frames + 2);
            }
        } else if(params.spatial_filter == spatial_filter_type::GAUSSIAN) {
            original_temporal_buffer = std::vector<cv::Mat_<float>>(1);
            processed_temporal_buffer = std::vector<cv::Mat_<float>>(1);
            fftwf_buffer = std::vector<cv::Mat_<float>>(1);
            current_layers.resize(params.n_layers - 1);
            original_temporal_buffer[0] = cv::Mat_<float>::zeros(
                    fit_to_layer(params.roi_rect.size(), params.n_layers-1).area() * params.n_channels,
                    params.n_buffered_frames);
            processed_temporal_buffer[0] = cv::Mat_<float>::zeros(
                    fit_to_layer(params.roi_rect.size(), params.n_layers-1).area() * params.n_channels,
                    params.n_buffered_frames);
            fftwf_buffer[0] = cv::Mat_<float>::zeros(
                    fit_to_layer(params.roi_rect.size(), params.n_layers-1).area() * params.n_channels,
                    params.n_buffered_frames + 2);
        }
    } else {
        current_layers.resize(params.n_layers);
        lowpassLo.resize(params.n_layers);
        lowpassHi.resize(params.n_layers);
        for (int layer_id = 0; layer_id < params.n_layers; ++layer_id) {
            lowpassLo[layer_id] = cv::Mat_<cv::Vec3f>::zeros(fit_to_layer(params.roi_rect.size(), layer_id));
            lowpassHi[layer_id] = cv::Mat_<cv::Vec3f>::zeros(fit_to_layer(params.roi_rect.size(), layer_id));
        }
    }
    average_roi_pixels = cv::Mat_<float>::zeros(params.n_channels, params.n_buffered_frames);
}
