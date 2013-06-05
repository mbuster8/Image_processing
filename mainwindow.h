#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <QtGui>
#include <QtCore>
#include <QMutex>
#include "My_label.h"
#include "mythread.h"

using namespace std;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();
    void resizeEvent(QResizeEvent *);
    void closeEvent(QCloseEvent *);
    void display_on_label();
    cv::Mat scale(const cv::Mat& image);

    signals:
    void picture_changed(QImage image, int picture_ID);

    private slots:
        void reload_picture();
        void Mouse_position_Slot(int pos_x, int pos_y);
        void receive_new_image(QString name, cv::Mat received_mat);
        void on_actionOpen_triggered();

private:
        Ui::MainWindow *ui;
        My_label *my_label;
        QLabel *des;
        MyThread *my_thread_1;
        QVBoxLayout *left_panel_label_layout;
        QMutex mutex;

        QList <cv::Mat> Qlist_of_pictures;
        QList <QLabel*> Qlist_of_descriptions;
        QList <My_label*> Qlist_of_labels;

        int actual_image_ID;
        int max_num_of_pictures;
        int picture_counter;
        int label_description_height;
        int representative_label_height, representative_label_width;
};

#endif // MAINWINDOW_H
