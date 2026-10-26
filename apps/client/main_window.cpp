#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow){
    ui->setupUi(this);
    counter = 0;
}

MainWindow::~MainWindow(){
    delete ui;
}

cv::Mat MainWindow::scale(const cv::Mat &image){
    double min = 0, max = 1;
    cv::minMaxLoc(image, &min, &max);
    cv::Mat res;
    cv::Mat tmp = ((image - min) / (max - min)) * 255;
    tmp.convertTo(res, CV_8UC1);
    return res;
}

void MainWindow::on_btn_open_clicked(){

    QString file_name = QFileDialog::getOpenFileName(this,tr("Open File"),"/home",
    tr("Image Files(*.png *.bmp *.jpg *.jpeg *.gif *.tiff *.ppm *.pgm *.pbm *.xbm *.xpm)"));
    //QFile file(file_name);

    if (file_name.size()<1) return; //if no file opened, nothing is going to be processed
    actual_image = cv::imread(file_name.toAscii().data(),CV_LOAD_IMAGE_UNCHANGED);  
    original_image = actual_image.clone();

    actual_image = scale(actual_image);

    if (actual_image.channels() == 3)cv::cvtColor(actual_image,actual_image,CV_BGR2RGB);
    else cv::cvtColor(actual_image,actual_image,CV_GRAY2RGB);

    QImage q_image = QImage((const unsigned char*)(actual_image.data),actual_image.cols,actual_image.rows,actual_image.step[0],QImage::Format_RGB888);
    ui->lbl_obrazok->setPixmap(QPixmap::fromImage(q_image).scaled(ui->lbl_obrazok->width(),ui->lbl_obrazok->height(),Qt::KeepAspectRatio));
}

void MainWindow::on_btn_send_image_clicked(){
    for (int i = 1;i <=1;i++){
        counter++;
        QString number = QString::number(counter);
        std::string picture_name("môj obrázok č.");
        picture_name.append(number.toStdString());
        send_image(original_image,picture_name);
    }
}
