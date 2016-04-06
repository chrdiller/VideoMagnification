#include <include/processing/analysis.h>

#include <fftw3.h>

namespace analysis {

    analysis_data analyze_heartbeat(parameter_store& params, DataContainer& data_container) {
        cv::Mat_<float> averaged_pixels = data_container.get_average_roi_pixels();

        //fftwf forward produces buffered_frames/2+1 complex numbers, i.e. buffered_frames+2 floats
        cv::Mat_<float> fft_forward_output(params.n_channels, params.n_buffered_frames+2);

        static int n_frames_last_plan = 0;
        static fftwf_plan forward_plan;
        if(params.n_buffered_frames != n_frames_last_plan) {
            fftwf_destroy_plan(forward_plan);
            forward_plan = fftwf_plan_dft_r2c_1d(
                    params.n_buffered_frames,
                    averaged_pixels.ptr<float>(0),
                    reinterpret_cast<fftwf_complex *>(fft_forward_output.ptr<float>(0)),
                    FFTW_MEASURE);
            n_frames_last_plan = params.n_buffered_frames;
        }

        //Using double from here on as the plotting widget requires double precision
        cv::Mat_<double> magnitudes = cv::Mat_<double>::zeros(3, fft_forward_output.cols/2);
        for (int channel_id = 0; channel_id < params.n_channels; ++channel_id) {
            fftwf_execute_dft_r2c(forward_plan,
                                 averaged_pixels.ptr<float>(channel_id),
                                 reinterpret_cast<fftwf_complex*>(fft_forward_output.ptr<float>(channel_id)));
            //Manually calculate magnitudes while skipping DC component
            for(int i = 2; i < fft_forward_output.cols; i+=2) {
                magnitudes.at<double>(channel_id, i / 2) = std::sqrt(
                        std::pow(fft_forward_output.at<float>(channel_id, i), 2) +
                        std::pow(fft_forward_output.at<float>(channel_id, i + 1), 2));
            }
        }

        //Find the "best" channel (i.e. the channel with the lowest std deviation)
        int best_channel = 0;
        cv::Scalar mean, std_deviation, min_std_deviation;
        cv::meanStdDev(magnitudes.row(0), mean, std_deviation);
        min_std_deviation = std_deviation;
        for (int channel_id = 1; channel_id < params.n_channels; ++channel_id) {
            cv::meanStdDev(magnitudes.row(channel_id), mean, std_deviation);
            if (std_deviation[0] < min_std_deviation[0]) { //cv Scalar std_deviation only holds one value
                min_std_deviation = std_deviation;
                best_channel = channel_id;
            }
        }

        //Scale results to [0,1]
        double min, max;
        cv::minMaxLoc(averaged_pixels.row(best_channel), &min, &max);
        cv::Mat_<double> values_timedomain_mat = (averaged_pixels.row(best_channel) - min)/(max-min);

        int min_idx[2], max_idx[2];
        cv::minMaxIdx(magnitudes.row(best_channel), &min, &max, min_idx, max_idx);
        cv::Mat_<double> values_frequencydomain_mat = (magnitudes.row(best_channel) - min)/(max-min);

        //Calculate the current heartbeat
        double heartbeat_value = static_cast<double>(max_idx[1]);
        heartbeat_value *= static_cast<double>(params.fps);
        heartbeat_value /= static_cast<double>(params.n_buffered_frames);
        heartbeat_value *= 60.0;

        std::vector<double> values_timedomain(values_timedomain_mat.cols);
        values_timedomain_mat.copyTo(values_timedomain);
        std::vector<double> values_frequencydomain(values_frequencydomain_mat.cols);
        values_frequencydomain_mat.copyTo(values_frequencydomain);
        std::vector<double> keys_timedomain(values_timedomain_mat.cols);
        std::vector<double> keys_frequencydomain(values_frequencydomain_mat.cols);

        for(int i = 0; i < values_timedomain_mat.cols; ++i) {
            keys_timedomain[i] = static_cast<double>(i)/static_cast<double>(params.fps);
        }

        for(int i = 0; i < values_frequencydomain_mat.cols; ++i) {
            keys_frequencydomain[i] = static_cast<double>(i) * static_cast<double>(params.fps) /
                    static_cast<double>(params.n_buffered_frames);
        }

        return analysis_data {
                keys_timedomain,
                values_timedomain,
                keys_frequencydomain,
                values_frequencydomain,
                heartbeat_value
        };
    };
}
