#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <visual_debug.h>
#include <QtGui>
#include <QtCore>
#include <zmq.hpp>
#include <iostream>
#include <QMutex>
#include <QDebug>

using namespace std;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow{
    Q_OBJECT

    void send(cv::Mat image_to_send);
    cv::Mat scale(const cv::Mat &image);

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_btn_open_clicked();
    void on_btn_send_image_clicked();

private:
    Ui::MainWindow *ui;
    cv::Mat actual_image, original_image;
    int counter;
};

#endif // MAINWINDOW_H
