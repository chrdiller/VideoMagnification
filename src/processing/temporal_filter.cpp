#include <include/processing/temporal_filter.h>

#include <fftw3.h>

class fftw_forward_plans {
public:
    fftw_forward_plans() : plans(0) { }

    void update(DataContainer& data_container, int n_layers, int n_buffered_frames, bool measure) {
        if(plans.size() != n_layers || plan_buffered_frames != n_buffered_frames) {
            plans.resize(n_layers);
            for(int layer_id = 0; layer_id < n_layers; ++layer_id) {
                fftwf_destroy_plan(plans[layer_id]);
                plans[layer_id] = fftwf_plan_dft_r2c_1d(
                        plan_buffered_frames = n_buffered_frames,
                        data_container.get_input_timeseries(layer_id, 0, 0).ptr<float>(0),
                        reinterpret_cast<fftwf_complex*>(data_container.get_fftwf_data(layer_id, 0, 0).ptr<float>(0)),
                        measure ? FFTW_MEASURE : FFTW_ESTIMATE+FFTW_UNALIGNED);
            }
        }
    }

    fftwf_plan operator[](const int layer_id) { return plans[layer_id]; }

private:
    int plan_buffered_frames;
    std::vector<fftwf_plan> plans;
};

class fftw_backward_plans {
public:
    fftw_backward_plans() : plans(0) { }

    void update(DataContainer& data_container, int n_layers, int n_buffered_frames, bool measure) {
        if(plans.size() != n_layers || plan_buffered_frames != n_buffered_frames) {
            plans.resize(n_layers);
            for(int layer_id = 0; layer_id < n_layers; ++layer_id) {
                fftwf_destroy_plan(plans[layer_id]);
                plans[layer_id] = fftwf_plan_dft_c2r_1d(
                        plan_buffered_frames = n_buffered_frames,
                        reinterpret_cast<fftwf_complex *>(data_container.get_fftwf_data(layer_id, 0, 0).ptr<float>(0)),
                        data_container.get_output_timeseries(layer_id, 0, 0).ptr<float>(0),
                        measure ? FFTW_MEASURE : FFTW_ESTIMATE+FFTW_UNALIGNED);
            }
        }
    }

    fftwf_plan operator[](const int layer_id) { return plans[layer_id]; }

private:
    int plan_buffered_frames;
    std::vector<fftwf_plan> plans;
};

void temporal_filter::ideal_filter(parameter_store& params, DataContainer& data_container) {
    int n_layers = params.spatial_filter == spatial_filter_type::LAPLACIAN ? params.n_layers : 1;

    static fftw_forward_plans forward_plans;
    forward_plans.update(data_container, n_layers,
                         std::min(params.n_buffered_frames, data_container.get_n_used_frames()),
                         data_container.get_n_used_frames() >= params.n_buffered_frames);
    static fftw_backward_plans backward_plans;
    backward_plans.update(data_container, n_layers,
                          std::min(params.n_buffered_frames, data_container.get_n_used_frames()),
                          data_container.get_n_used_frames() >= params.n_buffered_frames);

    for (int layer_id = 0; layer_id < n_layers; ++layer_id) {
        float layer_lambda = sqrtf(powf(fit_to_layer(params.roi_rect.size(), layer_id).width, 2.f)
                                   + powf(fit_to_layer(params.roi_rect.size(), layer_id).height, 2.f));
        float calculated_alpha = layer_lambda / params.lambda_c * (1 + params.alpha);
        int n_timeseries = params.spatial_filter == spatial_filter_type::LAPLACIAN ?
                           fit_to_layer(params.roi_rect.size(), layer_id).area() :
                           fit_to_layer(params.roi_rect.size(), params.n_layers - 1).area();

#pragma omp parallel for collapse(2) shared(params, data_container, forward_plans, backward_plans)
        for (int timeseries_id = 0; timeseries_id < n_timeseries; ++timeseries_id) {
            for (int channel_id = 0; channel_id < params.n_channels; ++channel_id) {
                if (!params.active_channels[channel_id]) {
                    std::memcpy(
                            data_container.get_output_timeseries(layer_id, timeseries_id, channel_id).ptr<float>(0),
                            data_container.get_input_timeseries(layer_id, timeseries_id, channel_id).ptr<float>(0),
                            params.n_buffered_frames * sizeof(float));
                    continue;
                }

                fftwf_execute_dft_r2c(forward_plans[layer_id],
                        data_container.get_input_timeseries(layer_id, timeseries_id, channel_id).ptr<float>(0),
                        reinterpret_cast<fftwf_complex*>(data_container.get_fftwf_data(layer_id, timeseries_id,
                                                                                        channel_id).ptr<float>(0)));

                if(params.min_freq < params.max_freq)
                    data_container.get_fftwf_data(layer_id, timeseries_id, channel_id).colRange(
                            static_cast<int>(2.f * (params.min_freq / static_cast<float>(params.fps)) *
                                             static_cast<float>(params.n_buffered_frames)),
                            static_cast<int>(2.f * (params.max_freq / static_cast<float>(params.fps)) *
                                             static_cast<float>(params.n_buffered_frames))
                    ) *= calculated_alpha < params.alpha ? calculated_alpha : params.alpha;

                fftwf_execute_dft_c2r(backward_plans[layer_id],
                        reinterpret_cast<fftwf_complex*>(data_container.get_fftwf_data(layer_id, timeseries_id,
                                                                                        channel_id).ptr<float>(0)),
                        data_container.get_output_timeseries(layer_id, timeseries_id, channel_id).ptr<float>(0));

                data_container.get_output_timeseries(layer_id, timeseries_id, channel_id)
                        /= static_cast<float>(std::min(data_container.get_n_used_frames(), params.n_buffered_frames));
            }
        }
    }
}

void temporal_filter::iir_filter(parameter_store& params, DataContainer& data_container) {
#pragma omp parallel for shared(params, data_container)
    for(int layer_id = (params.spatial_filter == spatial_filter_type::GAUSSIAN ? params.n_layers-1 : 0); layer_id < params.n_layers; ++layer_id) {
        cv::Mat current_layer_data = data_container.get_layer(layer_id);

        float layer_lambda = sqrtf(powf(fit_to_layer(params.roi_rect.size(), layer_id).width, 2.f)
                                   + powf(fit_to_layer(params.roi_rect.size(), layer_id).height, 2.f));
        float calculated_alpha = layer_lambda / params.lambda_c * (1 + params.alpha);

        if (params.cutoffLo == 0.0) params.cutoffLo = 0.001;

        cv::Mat lowpassHi = data_container.get_lowpassHi(layer_id);
        cv::Mat lowpassLo = data_container.get_lowpassLo(layer_id);

        lowpassHi = (1 - params.cutoffHi) * lowpassHi + params.cutoffHi * current_layer_data;
        lowpassLo = (1 - params.cutoffLo) * lowpassLo + params.cutoffLo * current_layer_data;

        data_container.put_layer(layer_id, current_layer_data +
                (calculated_alpha < params.alpha ? calculated_alpha : params.alpha) * (lowpassHi - lowpassLo));
    }
}
