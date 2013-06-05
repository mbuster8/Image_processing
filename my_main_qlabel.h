#ifndef MY_MAIN_QLABEL_H
#define MY_MAIN_QLABEL_H

#include <QLabel>
#include <QMouseEvent>
#include <QEvent>
#include <QDebug>
#include <opencv2/core/core.hpp>

class my_main_qlabel : public QLabel{
    Q_OBJECT
public:
    explicit my_main_qlabel(QWidget *parent = 0);

    void mouseMoveEvent(QMouseEvent *ev);
    void wheelEvent(QWheelEvent *event);
    void zoom(double zoomfactor, int mouse_pos_x, int mouse_pos_y);

    QPixmap original_pixmap;  //we store here maximum number of pixels from the biggest possible image
    double scaleFactor_x, scalefactor_y;
    double zoom_of_picture;
    int offset_x;
    int offset_y;

private:
    
signals:
    void Mouse_position(int pos_x, int pos_y);
    void zoomed();
    
public slots:
    
};

#endif // MY_MAIN_QLABEL_H
