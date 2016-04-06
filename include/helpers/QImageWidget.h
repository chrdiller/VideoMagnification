
#ifndef QIMAGEWIDGET_H
#define QIMAGEWIDGET_H

#include <opencv2/core.hpp>

#include <QtWidgets/QLabel>
#include <QtCore/QFlags>
#include <helpers/common.h>

class QImageWidget : public QLabel {
    Q_OBJECT

public:
    QImageWidget(QWidget *parent = 0, Qt::WindowFlags f = 0);

    void mousePressEvent(QMouseEvent* ev);
    void mouseReleaseEvent(QMouseEvent* ev);
    void mouseMoveEvent(QMouseEvent *ev);

    void imshow(const cv::Mat image);
    void show_text(std::string text);

private:
    mouse_selection selection;

signals:
    void area_selected(const mouse_selection& selection);
    void new_frame(QPixmap pixmap);

private slots:
    void got_new_frame(QPixmap pixmap);
};

#endif //QIMAGEWIDGET_H
