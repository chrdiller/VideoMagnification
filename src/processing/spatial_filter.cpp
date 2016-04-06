#include <include/processing/spatial_filter.h>

void spatial_filter::spatial_decomp(parameter_store& params, DataContainer& data_container) {
    cv::Mat last_layer = data_container.get_frame_roi();
    cv::Mat scaled_down = last_layer, scaled_up;
    for (int layer_id = 0; layer_id < params.n_layers-1; ++layer_id) {
        cv::pyrDown(scaled_down, scaled_down);
        cv::pyrUp(scaled_down, scaled_up);
        data_container.put_layer(layer_id, last_layer-scaled_up);
        last_layer = scaled_down;
    }
    data_container.put_layer(params.n_layers-1, last_layer);
}

void spatial_filter::spatial_comp(parameter_store& params, DataContainer& data_container) {
    for(int layer_id = 0; layer_id < params.n_layers; ++layer_id) {
        cv::Mat reconst_layer = data_container.get_layer(layer_id);
        for (int inner_layer_id = layer_id; inner_layer_id > 0; --inner_layer_id)
            cv::pyrUp(reconst_layer, reconst_layer);
        data_container.insert_reconstructed_layer_roi(reconst_layer);
    }
}
