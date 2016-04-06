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

//STL
#include <string>
#include <thread>

//OpenCV
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>

//QT5
#include <QApplication>
#include <QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLCDNumber>
#include <QtWidgets/QMessageBox>

//Other 3rd party
#include <qcustomplot.h>
#include <iomanip>

//Project internal
#include <mainwindow.h>
#include <video_source.h>
#include <include/processing/spatial_filter.h>
#include <include/processing/temporal_filter.h>
#include <include/processing/analysis.h>
#include <helpers/QImageWidget.h>

using std::string;

//Set all GUI elements to their corresponding values stored in the current parameter_store
void sync_gui_with_parameter_store(MainWindow& window, parameter_store& params) {
    //Filter types
    if(params.spatial_filter == spatial_filter_type::NONE)
        window.findChild<QComboBox*>("cb_spatialFilter")->setCurrentIndex(0);
    else if(params.spatial_filter == spatial_filter_type::GAUSSIAN)
        window.findChild<QComboBox*>("cb_spatialFilter")->setCurrentIndex(1);
    else if(params.spatial_filter == spatial_filter_type::LAPLACIAN)
        window.findChild<QComboBox*>("cb_spatialFilter")->setCurrentIndex(2);

    if(params.temporal_filter == temporal_filter_type::IDEAL)
        window.findChild<QComboBox*>("cb_temporalFilter")->setCurrentIndex(0);
    else if(params.temporal_filter == temporal_filter_type::IIR)
        window.findChild<QComboBox*>("cb_temporalFilter")->setCurrentIndex(1);

    //Color
    switch(params.color_convert_forward) {
        case -1: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(0); break;
        case CV_BGR2XYZ: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(1); break;
        case CV_BGR2YCrCb: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(2); break;
        case CV_BGR2HSV: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(3); break;
        case CV_BGR2Lab: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(4); break;
        case CV_BGR2Luv: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(5); break;
        case CV_BGR2YUV: window.findChild<QComboBox*>("cb_colorSpace")->setCurrentIndex(6); break;
        default: break;
    }
    window.findChild<QCheckBox*>("chb_channelOne")->setChecked(params.active_channels[0]);
    window.findChild<QCheckBox*>("chb_channelTwo")->setChecked(params.active_channels[1]);
    window.findChild<QCheckBox*>("chb_channelThree")->setChecked(params.active_channels[2]);

    //Spatial filter parameters
    window.findChild<QSpinBox*>("sb_nBufferedSeconds")->setValue(params.n_buffered_frames / params.fps);
    window.findChild<QSpinBox*>("sb_nLayers")->setValue(params.n_layers);

    //Temporal filter parameters
    window.findChild<QSlider*>("sld_alpha")->setValue(static_cast<int>(params.alpha));
    window.findChild<QSlider*>("sld_lambda_c")->setValue(static_cast<int>(params.lambda_c));
    float min_freq_max, max_freq_max, alpha_max, lambda_c_max;
    if(params.temporal_filter == temporal_filter_type::IDEAL) {
        min_freq_max = params.fps/2; max_freq_max = params.fps/2; alpha_max = 200.f; lambda_c_max = 1000.f;
        window.findChild<QLabel*>("lbl_minFreq_text")->setText("Min frequency in Hz");
        window.findChild<QSlider *>("sld_min_freq")->setValue(static_cast<int>(params.min_freq * 10.f));
        window.findChild<QLabel*>("lbl_maxFreq_text")->setText("Max frequency in Hz");
        window.findChild<QSlider *>("sld_max_freq")->setValue(static_cast<int>(params.max_freq * 10.f));

    } else if(params.temporal_filter == temporal_filter_type::IIR) {
        min_freq_max = 1.f; max_freq_max = 1.f; alpha_max = 200.f; lambda_c_max = 1000.f;
        window.findChild<QLabel*>("lbl_minFreq_text")->setText("Lower cutoff in %");
        window.findChild<QSlider *>("sld_min_freq")->setValue(static_cast<int>(params.cutoffLo * 10.f));
        window.findChild<QLabel*>("lbl_maxFreq_text")->setText("Higher cutoff in %");
        window.findChild<QSlider *>("sld_max_freq")->setValue(static_cast<int>(params.cutoffHi * 10.f));
    }
    std::stringstream ss; ss << std::fixed << std::setprecision(2) << min_freq_max;
    window.findChild<QLabel*>("lbl_minFreq_max")->setText(QString::fromStdString(ss.str()));
    ss.str(std::string()); ss << std::fixed << std::setprecision(2) << max_freq_max;
    window.findChild<QLabel*>("lbl_maxFreq_max")->setText(QString::fromStdString(ss.str()));
    ss.str(std::string()); ss << std::fixed << std::setprecision(2) << alpha_max;
    window.findChild<QLabel*>("lbl_alpha_max")->setText(QString::fromStdString(ss.str()));
    ss.str(std::string()); ss << std::fixed << std::setprecision(2) << lambda_c_max;
    window.findChild<QLabel*>("lbl_lambda_c_max")->setText(QString::fromStdString(ss.str()));
    window.findChild<QSlider*>("sld_min_freq")->setMaximum(static_cast<int>(min_freq_max * 10.f));
    window.findChild<QSlider*>("sld_max_freq")->setMaximum(static_cast<int>(max_freq_max * 10.f));
    window.findChild<QSlider*>("sld_alpha")->setMaximum(static_cast<int>(alpha_max));
    window.findChild<QSlider*>("sld_lambda_c")->setMaximum(static_cast<int>(lambda_c_max));

    window.findChild<QCheckBox*>("chb_analyzeHeartbeat")->setChecked(params.analyze_heartbeat);
}

void reset_parameters(parameter_store& params) {
    //Filter types
    params.spatial_filter = spatial_filter_type::NONE; //Reset all parameters and sync with GUI
    params.temporal_filter = temporal_filter_type::IDEAL;

    //Color
    params.color_convert_forward = -1;
    params.color_convert_backward = CV_BGR2RGB;
    params.active_channels = std::vector<bool>{true, true, true};

    //Spatial filter parameters
    params.roi_rect = cv::Rect(0,0,0,0);
    params.n_buffered_frames = 5; //Seconds, will be multiplied by fps
    params.n_layers = 3;

    //Temporal filter parameters
    params.alpha = 50.f;
    params.lambda_c = 100.f;
    params.min_freq = 1.f;
    params.max_freq = 2.f;
    params.cutoffLo = .25f;
    params.cutoffHi = .6f;

    //Video output parameters
    params.write_to_file = false;
    params.convert_whole_video = false;
    params.output_fourcc = cv::VideoWriter::fourcc('M','P','4','2');
    std::string video_output_filename = "";

    //Misc
    params.fps = 1;
    params.n_channels = 3;
    params.analyze_heartbeat = false;
    params.shutdown = false;
}

//Handle an error by displaying a simple message box
void handle_error(string message) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setModal(true);
    msgBox.setWindowTitle("Houston, we have a problem");
    msgBox.setText(QString::fromStdString(message));
    msgBox.exec();
}

//Enable or disable all GUI elements
void set_gui_enabled(bool enabled, MainWindow& window) {
    window.findChild<QWidget*>("tab_videoInput")->setEnabled(enabled);
    window.findChild<QWidget*>("tab_videoMagnification")->setEnabled(enabled);
    window.findChild<QWidget*>("tab_heartbeatAnalysis")->setEnabled(enabled);
    window.findChild<QWidget*>("tab_videoOutput")->setEnabled(enabled);
    window.findChild<QWidget*>("tab_cameraParameters")->setEnabled(enabled);
}

//Simple helper to clear a QT VBoxLayout
inline void clear_vbox_layout(QVBoxLayout* layout) {
    QLayoutItem* child;
    while(layout->count() != 0) {
        child = layout->takeAt(0);
        layout->removeItem(child);
        delete child;
    }
}

void setup_v4l2_controls(VideoSource& video_source, MainWindow& window) {
    //Get all supported camera options
    auto options = video_source.list_options();

    //Setup the layout container
    QVBoxLayout* camera_parameters_container = window.findChild<QVBoxLayout*>("layout_camera_parameters");
    clear_vbox_layout(camera_parameters_container);
    camera_parameters_container->setAlignment(Qt::AlignCenter);

    //No camera options available
    if(options.size() == 0) {
        camera_parameters_container->addWidget(new QLabel("Camera options not supported for the current stream"));
        return;
    }

    //Add GUI elements for all options
    for(auto option : options) {
        if(option.type == v4l2_option_type::INTEGER || option.type == v4l2_option_type::MENU) {
            QHBoxLayout* slider_layout = new QHBoxLayout; //Combines name label, slider and value label

            QLabel* name_label = new QLabel(QString::fromStdString(option.name)); //Setup labels
            QLabel* value_label = new QLabel(QString::fromStdString(std::to_string(option.current)));

            QSlider* slider = new QSlider(Qt::Orientation::Horizontal);
            slider->setMinimum(option.minimum); //Setup slider
            slider->setMaximum(option.maximum);
            slider->setValue(option.current);
            slider->setTracking(false);

            slider_layout->addWidget(name_label);
            slider_layout->addWidget(slider); //Add these widgets to layout
            slider_layout->addWidget(value_label);

            QObject::connect(
                    slider, //Connect to functionality
                    static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
                            [value_label, slider, &video_source, option](int value) {
                                int set_value = video_source.set_option_value(option, value);
                                slider->setValue(set_value);
                                value_label->setText(QString::fromStdString(std::to_string(set_value)));
                            });

            camera_parameters_container->addLayout(slider_layout); //Add the slider layout to the global layout
        } else if(option.type == v4l2_option_type::BOOLEAN) {
            QCheckBox* checkbox = new QCheckBox(QString::fromStdString(option.name));
            checkbox->setChecked(option.current);

            QObject::connect(
                    checkbox, //Connect to functionality
                    static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged),
                    [checkbox, &video_source, option](int state){
                        int set_value = video_source.set_option_value(option, state);
                        checkbox->setChecked(set_value);
                    });

            camera_parameters_container->addWidget(checkbox); //Add the checkbox to the global layout
        }
    }
}

/**
* Detects a single(!) face in a given image using OpenCV's CascadeClassifier and a pretrained classifier
* All detections are combined into one cv::Rect; this is then returned
* A rect with the size of the input image is returned if no face has been detected
*/
cv::Rect simple_face_detection(const cv::Mat& image) noexcept {
    std::vector<cv::Rect> detections;
    cv::CascadeClassifier classifier(FACE_CLASSIFIER_FILE); //Macro will be set by CMake
    classifier.detectMultiScale(image, detections);
    if(detections.size() == 0)
        return cv::Rect(0, 0, image.cols, image.rows);
    cv::Rect detection_rect = detections[0];
    for(int detection_id = 1; detection_id < detections.size(); ++detection_id)
        detection_rect = detection_rect | detections[detection_id];
    return detection_rect;
}

int main(int argc, char** argv) {
    //QApplication setup
    QApplication a(argc, argv);
    MainWindow window;
    window.show();

    //Add custom frame widget for live preview
    QImageWidget live_preview_image_widget;
    window.findChild<QHBoxLayout*>("layout_output")->setAlignment(Qt::AlignCenter);
    window.findChild<QHBoxLayout*>("layout_output")->addWidget(&live_preview_image_widget);
    live_preview_image_widget.show_text("No video loaded");

    //Add custom plot widgets
    QCustomPlot custom_plot_time, custom_plot_frequency;
    custom_plot_time.setFixedHeight(150); custom_plot_frequency.setFixedHeight(150);
    window.findChild<QVBoxLayout*>("layout_heartbeatAnalysis")->addWidget(&custom_plot_time);
    window.findChild<QVBoxLayout*>("layout_heartbeatAnalysis")->addWidget(&custom_plot_frequency);

    //The graph used to display timedomain values
    QCPGraph* time_graph = custom_plot_time.addGraph();
    time_graph->keyAxis()->setLabel("seconds");
    time_graph->valueAxis()->setLabel("intensity");

    //The graph used to display frequencydomain values
    QCPBars* frequency_bars = new QCPBars(custom_plot_frequency.xAxis, custom_plot_frequency.yAxis);
    custom_plot_frequency.addPlottable(frequency_bars);
    frequency_bars->keyAxis()->setLabel("frequency in Hz");
    frequency_bars->valueAxis()->setLabel("intensity");
    frequency_bars->setPen(Qt::NoPen);
    frequency_bars->setBrush(QColor(0, 0, 160));

    VideoSource video_source; //Video input
    cv::VideoWriter video_writer; //Video output

    parameter_store params; //Global parameter store

    std::thread processing_thread;

    //Mouse selection handling
    mouse_selection selection;
    QObject::connect(&live_preview_image_widget,
                     &QImageWidget::area_selected,
                     [&selection](const mouse_selection& _selection) {
                        selection = _selection;
    });

    auto main_lambda = [&params, &selection, &processing_thread,
            &video_source, &video_writer,
            &window, &live_preview_image_widget, &custom_plot_time, &custom_plot_frequency,
            time_graph, frequency_bars]() {
        params.shutdown = false;
        processing_thread = std::thread([&params, &selection,
                        &video_source, &video_writer,
                        &window, &live_preview_image_widget, &custom_plot_time, &custom_plot_frequency,
                        time_graph, frequency_bars]() {
            cv::Mat frame;
            cv::Rect selection_rect;
            cv::Scalar roi_color; //Use different colors to draw the selected roi
            DataContainer data_container(params);
            parameter_store buffered_params;
            set_gui_enabled(true, window);
            while(!params.shutdown) {
                if(params.n_layers != buffered_params.n_layers) //Re-align the roi rect if number of layers has changed
                    params.roi_rect = align_rect(params.roi_rect, params.n_layers);
                buffered_params = params; //Buffer params per frame

                auto start = std::chrono::high_resolution_clock::now();

                video_source >> frame;

                if (buffered_params.color_convert_forward > 0)
                    cv::cvtColor(frame, frame, buffered_params.color_convert_forward);

                if(!selection.complete && !selection.selecting &&
                        buffered_params.spatial_filter == spatial_filter_type::NONE) {
                    selection_rect = params.roi_rect = buffered_params.roi_rect =
                            align_rect(simple_face_detection(frame), buffered_params.n_layers);
                    roi_color = cv::Scalar(0, 0, 255);
                } else if(selection.fresh && selection.complete && !selection.selecting) {
                    selection_rect = params.roi_rect = buffered_params.roi_rect =
                            align_rect(selection.rect(), buffered_params.n_layers);
                    roi_color = cv::Scalar(0, 255, 0);
                } else if(selection.fresh && !selection.complete && selection.selecting) {
                    selection_rect = selection.rect();
                    roi_color = cv::Scalar(255, 255, 0);
                } else {
                    selection_rect = buffered_params.roi_rect;
                }

                frame.convertTo(frame, CV_32FC3);
                data_container.push_frame(frame, buffered_params);

                if(buffered_params.spatial_filter != spatial_filter_type::NONE) {
                    if (buffered_params.temporal_filter == temporal_filter_type::IDEAL) {
                        spatial_filter::spatial_decomp(buffered_params, data_container);
                        temporal_filter::ideal_filter(buffered_params, data_container);
                        spatial_filter::spatial_comp(buffered_params, data_container);
                    } else if(buffered_params.temporal_filter == temporal_filter_type::IIR) {
                        spatial_filter::spatial_decomp(buffered_params, data_container);
                        temporal_filter::iir_filter(buffered_params, data_container);
                        spatial_filter::spatial_comp(buffered_params, data_container);
                    }
                }

                data_container.pop_frame().convertTo(frame, CV_8UC3);

                if (buffered_params.analyze_heartbeat) {
                    auto analysis_result = analysis::analyze_heartbeat(buffered_params, data_container);

                    time_graph->keyAxis()->setRange(0, analysis_result.timedomain_keys[analysis_result.timedomain_keys.size()-1]);
                    time_graph->valueAxis()->setRange(0, 1);
                    time_graph->setData(QVector<double>::fromStdVector(analysis_result.timedomain_keys),
                                        QVector<double>::fromStdVector(analysis_result.timedomain_values));
                    custom_plot_time.replot();

                    frequency_bars->keyAxis()->setRange(0, analysis_result.frequencydomain_keys[analysis_result.frequencydomain_keys.size()-1]);
                    frequency_bars->valueAxis()->setRange(0, 1);
                    frequency_bars->setData(QVector<double>::fromStdVector(analysis_result.frequencydomain_keys),
                                            QVector<double>::fromStdVector(analysis_result.frequencydomain_values));
                    custom_plot_frequency.replot();

                    window.findChild<QLCDNumber*>("lcd_heartbeatNumber")->display(analysis_result.heartbeat_number);
                }

                cv::cvtColor(frame, frame, buffered_params.color_convert_backward);
                cv::rectangle(frame, selection_rect, roi_color); //Draw to preview widget
                live_preview_image_widget.imshow(frame);

                if(buffered_params.write_to_file) {
                    if(buffered_params.convert_whole_video && !video_source.is_first_playback()) {
                        params.write_to_file = false;
                        video_writer.release();
                        params.video_output_filename = "";
                        window.findChild<QPushButton*>("btn_startStopConvertVideo")->setText("Convert whole video");
                        window.findChild<QLabel*>("lbl_outputFilename")->setText("No file selected");
                        set_gui_enabled(true, window);
                    } else {
                        cv::cvtColor(frame, frame, CV_RGB2BGR);
                        video_writer.write(frame);
                    }
                }

                auto end = std::chrono::high_resolution_clock::now();
                if(!buffered_params.write_to_file || !buffered_params.convert_whole_video)
                    std::this_thread::sleep_for(std::chrono::duration<int, std::ratio<1,1000>>(1000/buffered_params.fps)-(end-start));
            }
        });
        processing_thread.detach();
    };

    //### The following section contains all the code necessary to handle incoming GUI events ###
    //## Tab "Video Input"
    //Clicked "Load video device"
    QObject::connect(
            window.findChild<QPushButton*>("btn_loadVideoDevice"),
            &QPushButton::clicked,
            [&params, &window, &main_lambda, &video_source, &live_preview_image_widget, &processing_thread]() {
                set_gui_enabled(false, window);
                live_preview_image_widget.show_text("Shutting down current processing thread ...");

                params.shutdown = true; //Shutdown any running processing thread
                if(processing_thread.joinable())
                    processing_thread.join();

                live_preview_image_widget.show_text("Loading video ...");

                video_source.release(); //Re-start the current video source
                int video_device_id = window.findChild<QSpinBox*>("sb_deviceID")->value();
                bool open_success = video_source.open(video_device_id);
                if(open_success) {
                    window.findChild<QPushButton*>("btn_startStopWriteStream")->setEnabled(true);
                    window.findChild<QPushButton*>("btn_startStopConvertVideo")->setEnabled(false);

                    reset_parameters(params);

                    params.fps = video_source.get_fps(); //Get fps and set number of buffered frames to the correct value
                    params.n_buffered_frames *= params.fps;

                    sync_gui_with_parameter_store(window, params);

                    setup_v4l2_controls(video_source, window);
                    main_lambda(); //Start processing
                } else {
                    handle_error("Could not open video device " + std::to_string(video_device_id));
                    live_preview_image_widget.show_text("No video loaded");
                    window.findChild<QWidget*>("tab_videoInput")->setEnabled(true);
                }
    });

    //Clicked "Select video file"
    QObject::connect(
            window.findChild<QPushButton*>("btn_selectVideoInputFile"),
            &QPushButton::clicked,
            [&params, &window, &main_lambda, &video_source, &live_preview_image_widget, &processing_thread]() {
                std::string video_filename =
                        QFileDialog::getOpenFileName(window.findChild<QPushButton*>("btn_selectVideoOutputFile"), //Parent
                                                     "Open Video File", //Dialog title
                                                     "", //Suggested directory
                                                     "Video Containers (*.mp4 *.avi *.mov *.mkv)" //Filter
                        ).toStdString();
                if(video_filename == "") return; //Don't resume if file dialog has been cancelled

                set_gui_enabled(false, window);
                live_preview_image_widget.show_text("Shutting down current processing thread ...");

                params.shutdown = true; //Shutdown any running processing thread
                if(processing_thread.joinable())
                    processing_thread.join();

                live_preview_image_widget.show_text("Loading video ...");

                video_source.release(); //Re-start the current video source
                bool open_success = video_source.open(video_filename);
                if(open_success) {
                    window.findChild<QPushButton*>("btn_startStopWriteStream")->setEnabled(true);
                    window.findChild<QPushButton*>("btn_startStopConvertVideo")->setEnabled(true);

                    reset_parameters(params);

                    params.fps = video_source.get_fps(); //Get fps and set number of buffered frames to the correct value
                    params.n_buffered_frames *= params.fps;

                    sync_gui_with_parameter_store(window, params);

                    setup_v4l2_controls(video_source, window);

                    main_lambda(); //Start processing
                } else {
                    handle_error("Could not open video file " + video_filename);
                    live_preview_image_widget.show_text("No video loaded");
                    window.findChild<QWidget*>("tab_videoInput")->setEnabled(true);
                }
    });

    //Chose different color space
    QObject::connect(
            window.findChild<QComboBox*>("cb_colorSpace"),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [&params](int index){
                switch(index) {
                    case 0: params.color_convert_forward = -1; params.color_convert_backward = CV_BGR2RGB; break;
                    case 1: params.color_convert_forward = CV_BGR2XYZ; params.color_convert_backward = CV_XYZ2RGB; break;
                    case 2: params.color_convert_forward = CV_BGR2YCrCb; params.color_convert_backward = CV_YCrCb2RGB; break;
                    case 3: params.color_convert_forward = CV_BGR2HSV; params.color_convert_backward = CV_HSV2RGB; break;
                    case 4: params.color_convert_forward = CV_BGR2Lab; params.color_convert_backward = CV_Lab2RGB; break;
                    case 5: params.color_convert_forward = CV_BGR2Luv; params.color_convert_backward = CV_Luv2RGB; break;
                    case 6: params.color_convert_forward = CV_BGR2YUV; params.color_convert_backward = CV_YUV2RGB; break;
                    default: break;
                }
    });

    //(Un)checked channel one
    QObject::connect(
            window.findChild<QCheckBox*>("chb_channelOne"),
            static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged),
            [&params](int state){
                params.active_channels[0] = (state == Qt::Checked);
    });

    //(Un)checked channel two
    QObject::connect(
            window.findChild<QCheckBox*>("chb_channelTwo"),
            static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged),
            [&params](int state){
                params.active_channels[1] = (state == Qt::Checked);
    });

    //(Un)checked channel three
    QObject::connect(
            window.findChild<QCheckBox*>("chb_channelThree"),
            static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged),
            [&params](int state){
                params.active_channels[2] = (state == Qt::Checked);
    });

    //## Tab "Video Magnification"
    //Chose different spatial filter type
    QObject::connect(
            window.findChild<QComboBox*>("cb_spatialFilter"),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [&params, &window](int index){
                if(index == 0)
                    params.spatial_filter = spatial_filter_type::NONE;
                if(index == 1)
                    params.spatial_filter = spatial_filter_type::GAUSSIAN;
                if(index == 2)
                    params.spatial_filter = spatial_filter_type::LAPLACIAN;
                sync_gui_with_parameter_store(window, params);
    });

    //Chose different temporal filter type
    QObject::connect(
            window.findChild<QComboBox*>("cb_temporalFilter"),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [&params, &window](int index){
                if(index == 0)
                    params.temporal_filter = temporal_filter_type::IDEAL;
                if(index == 1)
                    params.temporal_filter = temporal_filter_type::IIR;
                sync_gui_with_parameter_store(window, params);
    });

    //Adjusted value of minimal frequency for ideal magnification
    QObject::connect(
            window.findChild<QSlider*>("sld_min_freq"),
            static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
            [&params, &window](int value) { //QSlider does not support floating point numbers
                params.min_freq = static_cast<float>(value) / 10.f;
                params.cutoffLo = static_cast<float>(value) / 10.f;
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << params.min_freq;
                window.findChild<QLabel*>("lbl_minFreq")->setText(QString::fromStdString(ss.str()));
    });

    //Adjusted value of maximal frequency for ideal magnification
    QObject::connect(
            window.findChild<QSlider*>("sld_max_freq"),
            static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
            [&params, &window](int value) { //QSlider does not support floating point numbers
                params.max_freq = static_cast<float>(value) / 10.f;
                params.cutoffHi = static_cast<float>(value) / 10.f;
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << params.max_freq;
                window.findChild<QLabel*>("lbl_maxFreq")->setText(QString::fromStdString(ss.str()));
    });

    //Adjusted alpha value for ideal magnification
    QObject::connect(
            window.findChild<QSlider*>("sld_alpha"),
            static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
            [&params, &window](int value) {
                params.alpha = static_cast<float>(value);
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << params.alpha;
                window.findChild<QLabel*>("lbl_alpha")->setText(QString::fromStdString(ss.str()));
    });

    //Adjusted lambda cutoff value for ideal magnification
    QObject::connect(
            window.findChild<QSlider*>("sld_lambda_c"),
            static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
            [&params, &window](int value) {
                params.lambda_c = static_cast<float>(value);
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << params.lambda_c;
                window.findChild<QLabel*>("lbl_lambda_c")->setText(QString::fromStdString(ss.str()));
    });

    //Adjusted number of seconds to buffer
    QObject::connect(
            window.findChild<QSpinBox*>("sb_nBufferedSeconds"),
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [&params](int value){
                params.n_buffered_frames = params.fps * value;
    });

    //Adjusted number of layers to consider
    QObject::connect(
            window.findChild<QSpinBox*>("sb_nLayers"),
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [&params](int value){
                params.n_layers = value;
    });

    //## Tab "Heartbeat Analysis"
    //(Un)checked whether to perform heartbeat analysis
    QObject::connect(
            window.findChild<QCheckBox*>("chb_analyzeHeartbeat"),
            static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged),
            [&params](int state){
                params.analyze_heartbeat = (state == Qt::Checked);
    });

    //## Tab "Video Output"
    //Clicked "Select filename"
    QObject::connect(
            window.findChild<QPushButton*>("btn_selectVideoOutputFile"),
            &QPushButton::clicked,
            [&params, &window](){
                params.video_output_filename = //Let the user choose a file
                        QFileDialog::getSaveFileName(window.findChild<QPushButton*>("btn_selectVideoOutputFile"), //Parent
                                                     "Write video to file", //Dialog title
                                                     "", //Suggested directory
                                                     "AVI Video Container (*.avi)" //Filter
                        ).toStdString();

                if(params.video_output_filename == "") return;
                if (params.video_output_filename.find(".avi") == -1)
                    params.video_output_filename += ".avi"; //If necessary, append video container extension
                window.findChild<QLabel*>("lbl_outputFilename")->setText(QString::fromStdString(params.video_output_filename));
    });

    //Chose different video compression
    QObject::connect(
            window.findChild<QComboBox*>("cb_outputCompression"),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [&params](int index){
                switch(index) {
                    case 0: params.output_fourcc = cv::VideoWriter::fourcc('H','2','6','4');
                    case 1: params.output_fourcc = cv::VideoWriter::fourcc('M','P','4','2');
                    case 2: params.output_fourcc = cv::VideoWriter::fourcc('D','I','V','X');
                    case 3: params.output_fourcc = cv::VideoWriter::fourcc('M','J','P','G');
                    default: break;
                }
    });

    //Clicked convert whole video
    QObject::connect(
            window.findChild<QPushButton*>("btn_startStopConvertVideo"),
            &QPushButton::clicked,
            [&params, &window, &video_writer, &video_source]() {
                if(params.video_output_filename == "") {
                    handle_error("Please select a video file first");
                } else {
                    params.write_to_file = !params.write_to_file;
                    params.convert_whole_video = true;
                    if (params.write_to_file) {
                        video_source.start_from_beginning();
                        video_writer.open(params.video_output_filename, params.output_fourcc,
                                          params.fps, video_source.get_frame_size());
                        window.findChild<QPushButton *>("btn_startStopConvertVideo")->setText("Stop");
                        params.n_buffered_frames = video_source.get_n_frames();
                        window.findChild<QWidget *>("tab_videoInput")->setEnabled(false);
                    } else {
                        video_writer.release();
                        params.video_output_filename = "";
                        window.findChild<QPushButton *>("btn_startStopConvertVideo")->setText("Convert whole video");
                        window.findChild<QLabel *>("lbl_outputFilename")->setText("No file selected");
                        window.findChild<QWidget *>("tab_videoInput")->setEnabled(true);
                    }
                }
    });

    //Clicked write current stream
    QObject::connect(
            window.findChild<QPushButton*>("btn_startStopWriteStream"),
            &QPushButton::clicked,
            [&params, &window, &video_writer, &video_source](){
                if(params.video_output_filename == "") {
                    handle_error("Please select a video file first");
                } else {
                    params.write_to_file = !params.write_to_file;
                    params.convert_whole_video = false;
                    if (params.write_to_file) {
                        video_writer.open(params.video_output_filename, params.output_fourcc,
                                          params.fps, video_source.get_frame_size());
                        window.findChild<QPushButton *>("btn_startStopWriteStream")->setText("Stop");
                        window.findChild<QWidget*>("tab_videoInput")->setEnabled(false);
                    } else {
                        video_writer.release();
                        params.video_output_filename = "";
                        window.findChild<QPushButton *>("btn_startStopWriteStream")->setText("Write current stream");
                        window.findChild<QLabel *>("lbl_outputFilename")->setText("No file selected");
                        window.findChild<QWidget*>("tab_videoInput")->setEnabled(true);
                    }
                }
    });

    //Start the main Qt event loop
    return a.exec();
}
