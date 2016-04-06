#include <helpers/QImageWidget.h>

#include <QtWidgets/QLabel>
#include <opencv2/imgproc.hpp>
#include <QtGui/QImage>
#include <qevent.h>
#include <helpers/common.h>
#include <qcoreevent.h>
#include <iostream>

QImageWidget::QImageWidget(QWidget *parent, Qt::WindowFlags f) : QLabel(parent, f) {
    setMouseTracking(true);
    QObject::connect(this, SIGNAL(new_frame(QPixmap)), SLOT(got_new_frame(QPixmap)));
}

void QImageWidget::mousePressEvent(QMouseEvent* ev) {
    selection.point_from.x = ev->x();
    selection.point_from.y = ev->y();
    selection.point_to.x = ev->x();
    selection.point_to.y = ev->y();
    selection.selecting = true;
    selection.complete = false;
    selection.fresh = true;
    emit area_selected(selection);
}

void QImageWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (selection.point_from.x == ev->x() && selection.point_from.y == ev->y()) {
        selection.selecting = false;
        selection.complete = false;
        selection.fresh = true;
    } else {
        selection.point_to.x = ev->x();
        selection.point_to.y = ev->y();
        selection.selecting = false;
        selection.complete = true;
        selection.fresh = true;
    }
    emit area_selected(selection);
}

void QImageWidget::mouseMoveEvent(QMouseEvent *ev) {
    if(!selection.selecting) return;
    selection.point_to.x = ev->x();
    selection.point_to.y = ev->y();
    selection.fresh = true;
    emit area_selected(selection);
}

void QImageWidget::imshow(const cv::Mat image) {
    QPixmap pixmap = QPixmap::fromImage(QImage(image.ptr<uchar>(0), image.cols, image.rows, image.step, QImage::Format_RGB888));
    emit new_frame(pixmap);
}

void QImageWidget::show_text(std::string text) {
    emit setText(QString::fromStdString(text));
}

void QImageWidget::got_new_frame(QPixmap pixmap) {
    this->setPixmap(pixmap);
}
