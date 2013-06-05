#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "My_label.h"
#include "my_main_qlabel.h"

static const char * mat_types[] = {
    "char", "uchar", "short", "ushort", "int", "float", "double", "user"
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),ui(new Ui::MainWindow){

    ui->setupUi(this);
    qRegisterMetaType<cv::Mat>("cv::Mat");  //we have to register cv::Mat type so that
                                            //Qt can recognize it and is able to connect
                                            //Signals to Slots when using this type
    my_thread_1 = new MyThread(this);

    connect(ui->lbl_Okno,SIGNAL(Mouse_position(int,int)),this,SLOT(Mouse_position_Slot(int,int)));
    connect(my_thread_1,SIGNAL(incoming_image(QString,cv::Mat)),this,
            SLOT(receive_new_image(QString,cv::Mat)));
    //connect(ui->lbl_Okno,SIGNAL(zoomed()),this,SLOT(Zoomed_slot()));

    my_thread_1->start(); //runs a new thread which launches server for
                          //inter-process communication and listens to requests

    //variables inicialization
    representative_label_width = 150;
    representative_label_height = 90;
    label_description_height = 30;
    max_num_of_pictures = 100;       //maximum number of pictures stored in memory - ringbuffer
    picture_counter = 0;            //variable counting number of received pictures

    //here we create a side panel of pictures
    //we put widgets into a layout so it makes no unexpected problems that while resizing
    left_panel_label_layout = new QVBoxLayout();
    left_panel_label_layout->setAlignment(Qt::AlignTop);
    left_panel_label_layout->setMargin(0);

    QWidget* left_panel_widget = new QWidget();
    left_panel_widget->setLayout(left_panel_label_layout);
    ui->lbl_Okno->setVisible(false);

    //we put our side panel to scrollarea to be easily able to scroll the pictures
    QScrollArea* left_panel_scrollarea = new QScrollArea(this);
    left_panel_scrollarea->setWidgetResizable(true);
    left_panel_scrollarea->setFixedWidth(representative_label_width + 18);
    left_panel_scrollarea->setBackgroundRole(QPalette::Light);
    left_panel_scrollarea->setWidget(left_panel_widget);
    ui->left_panel_layout->addWidget(left_panel_scrollarea);
}

MainWindow::~MainWindow(){
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *){
    //setting size policy of whole window followed by appropriate picture repainting on the label
    ui->lbl_Okno->setGeometry(0,0,ui->groupBox->width(),ui->groupBox->height());
    if (Qlist_of_pictures.size()>0) display_on_label();
}

void MainWindow::closeEvent(QCloseEvent *){
    this->my_thread_1->stop = true;
    this->my_thread_1->terminate();
}

void MainWindow::display_on_label(){
    ui->lbl_Okno->setVisible(true);

    mutex.lock();
        cv::Mat &image = Qlist_of_pictures[actual_image_ID];

        QImage q_image = QImage((const unsigned char*)image.data,
               image.cols,image.rows,image.step[0],QImage::Format_RGB888);

        //setting newly displayed picture's features to be displayed correctly
        ui->lbl_Okno->setPixmap(QPixmap::fromImage(q_image).scaled(ui->groupBox->width(),ui->groupBox->height(),Qt::KeepAspectRatio)); //Qt::KeepAspectRatiobyexpanding
        ui->lbl_Okno->setGeometry(0,0,ui->lbl_Okno->pixmap()->width(),ui->lbl_Okno->pixmap()->height());
        ui->lbl_Okno->original_pixmap = QPixmap::fromImage(q_image);
        ui->lbl_Okno->scalefactor_y = ui->lbl_Okno->scaleFactor_x =(double) ui->lbl_Okno->pixmap()->height() / (double) q_image.height();
        ui->lbl_Okno->zoom_of_picture = 1;
        ui->lbl_Okno->offset_x = 0;
        ui->lbl_Okno->offset_y = 0;

    mutex.unlock();
    emit picture_changed(q_image, actual_image_ID);
}

cv::Mat MainWindow::scale(const cv::Mat &image){
    double min = 0, max = 1;
    cv::minMaxLoc(image, &min, &max);
    cv::Mat res;
    cv::Mat tmp = ((image - min) / (max - min)) * 255;
    tmp.convertTo(res, CV_8UC1);
    return res;
}

void MainWindow::reload_picture(){  //ked klikne na label, nacita sa do pointra image pixmapa a yobrayi sa na label
    My_label *moj = (My_label *)sender();
    actual_image_ID = moj->get_identity();
    display_on_label();
}

void MainWindow::Mouse_position_Slot(int pos_x, int pos_y){

    QMutexLocker lock(&mutex);

        //counting accurate possition of mouse cursor on displayed image
        pos_x = (int)((pos_x / ui->lbl_Okno->scaleFactor_x) + ui->lbl_Okno->offset_x);
        pos_y = (int)((pos_y / ui->lbl_Okno->scalefactor_y) + ui->lbl_Okno->offset_y);

        cv::Mat& image = Qlist_of_pictures[actual_image_ID];

        QString values;

        //now we check every channel of displayed image at the position pointed by cursor
        // and add to variable "values" red, green and blue values of actual pixel
        //we have to recognize different types of images, therefore image.depth() is
        // assigned to variable type
        if (pos_x < image.cols && pos_y < image.rows) {
            int type = image.depth();
            int chans = image.channels();
            for (int ch = 0; ch < chans; ch++) {
                if (ch == 0) values.append(" | R=");
                else if (ch == 1) values.append(" G=");
                else if (ch == 2) values.append(" B=");
                if (type == CV_8U)
                    values.append(QString::number(image.at<unsigned char>(pos_y, pos_x*chans + ch)));
                else if (type == CV_8S)
                    values.append(QString::number(image.at<char>(pos_y, pos_x*chans + ch)));
                else if (type == CV_16S)
                    values.append(QString::number(image.at<short>(pos_y, pos_x*chans + ch)));
                else if (type == CV_16U)
                    values.append(QString::number(image.at<unsigned short>(pos_y, pos_x*chans + ch)));
                else if (type == CV_32S)
                    values.append(QString::number(image.at<int>(pos_y, pos_x*chans + ch)));
                else if (type == CV_32F)
                    values.append(QString::number((double) image.at<float>(pos_y, pos_x*chans + ch)));
                else
                    values.append(QString::number(image.at<double>(pos_y, pos_x*chans + ch)));
                values.append(" ");
            }
        }

        //setting text of actual postition and RGB values on the lbl_position label
        ui->lbl_position->setText(QString("Pos: X = %1, Y = %2 %3").arg(pos_x).arg(pos_y).arg(values));
}

void MainWindow::receive_new_image(QString name, cv::Mat received_mat){

    mutex.lock();

        //if our list of pictures is full, we rewrite the oldest picture in the list
        //- circular rewriting, we release representative labels out of the memory
        //just after we remove the labels from the left side panel
        if (Qlist_of_pictures.size()>=max_num_of_pictures){
            picture_counter = picture_counter % max_num_of_pictures;    //picture counter helps assign correct ID to label
            Qlist_of_pictures[picture_counter]= received_mat; //- circular rewriting
            left_panel_label_layout->removeWidget(Qlist_of_labels.at(0));
            left_panel_label_layout->removeWidget(Qlist_of_descriptions.at(0));
            delete Qlist_of_labels.takeFirst();         //FIFO queue - free memory block
            delete Qlist_of_descriptions.takeFirst();   //FIFO queue - free memory block
        }

        //if our list of pictures is not full, we add new picture to the list
        // adds new picture to list of pictures in original format and quality
        else Qlist_of_pictures.push_back(received_mat);

        //in channels we store original number of channels before scaling
        int channels = Qlist_of_pictures[picture_counter].channels();

        int displayable = 0;

        //in matrix_type we store type od data matrix is filled with -char,short, int, etc.
        const char * matrix_type;
        int type = Qlist_of_pictures[picture_counter].depth();

        if (type == CV_8S){
            matrix_type = mat_types[0];
            displayable = 1;
        }
        else if (type == CV_8U){
            matrix_type = mat_types[1];
            displayable = 1;
        }
        else if (type == CV_16S)matrix_type = mat_types[2];
        else if (type == CV_16U)matrix_type = mat_types[3];
        else if (type == CV_32S)matrix_type = mat_types[4];
        else if (type == CV_32F)matrix_type = mat_types[5];
        else if (type == CV_64F)matrix_type = mat_types[6];
        else matrix_type = mat_types[7];

        //we scale the picture so that if it is not displayable type we scale it to displayable
        if (displayable == 0) Qlist_of_pictures[picture_counter] = scale(Qlist_of_pictures[picture_counter].clone());

        //now we convert image format to suitable format for displaying our image
        //in actual_image we store the image we are working with
        if (Qlist_of_pictures[picture_counter].channels() == 3)
            cv::cvtColor(Qlist_of_pictures[picture_counter],Qlist_of_pictures[picture_counter],CV_BGR2RGB); //CV_BGR2RGB
        else
            cv::cvtColor(Qlist_of_pictures[picture_counter],Qlist_of_pictures[picture_counter],CV_GRAY2RGB); //CV_GRAY2RGB
        actual_image_ID = picture_counter;   //in actual_image we store the image we are working with

        //converting image to suitable format for qt environment
        QImage q_image = QImage((const unsigned char*)Qlist_of_pictures[picture_counter].data,
               Qlist_of_pictures[picture_counter].cols,Qlist_of_pictures[picture_counter].rows,Qlist_of_pictures[picture_counter].step[0],QImage::Format_RGB888);
        name+= "\n(" + QString::number(Qlist_of_pictures[picture_counter].cols) +
                "x" + QString::number(Qlist_of_pictures[picture_counter].rows) + "), "
                + QString::number(channels) + ", " + matrix_type;

        //creates new clickable label where the picture will be dislayed and sets its features
        my_label = new My_label(picture_counter,this);
        my_label->setFixedSize(representative_label_width,representative_label_height);
        my_label->setStyleSheet(ui->lbl_Okno->styleSheet());
        my_label->setFrameShape(ui->lbl_Okno->frameShape());
        my_label->setPixmap(QPixmap::fromImage(q_image).scaled(representative_label_width,representative_label_width,Qt::KeepAspectRatio));

        //creates picture description and sets its features
        des = new QLabel(this);
        des->setFixedSize(representative_label_width,label_description_height);
        des->setText(name);

        //setting signal-slot mechanism for new created label
        //if clicked on a picture on the left panel, we load a picture to the main window
        //if a picture in a main window is changed, we have to set new features to windows where displayed
        connect(my_label,SIGNAL(clicked()),this,SLOT(reload_picture()));
        connect(this,SIGNAL(picture_changed(QImage, int)),my_label,SLOT(reload_picture(QImage,int)));

        //adding new labels to suitable list do that we can anytime get them when needed
        Qlist_of_labels.push_back((my_label));
        Qlist_of_descriptions.push_back(des);

        //adding new created label/picture with description on the top of the side panel
        left_panel_label_layout->insertWidget(0,my_label);
        left_panel_label_layout->insertWidget(0,des);
        picture_counter++;

    mutex.unlock();

    display_on_label();
}

void MainWindow::on_actionOpen_triggered(){

    QString file_name = QFileDialog::getOpenFileName(this,tr("Open File"),"/home",
    tr("Image Files(*.png *.bmp *.jpg *.jpeg *.gif *.tiff *.ppm *.pgm *.pbm *.xbm *.xpm)"));
    //QFile file(file_name);

    if (file_name.size()<1) return; //if no file opened, nothing is going to be processed

    cv::Mat actual_image = cv::imread(file_name.toAscii().data(),CV_LOAD_IMAGE_UNCHANGED);
    receive_new_image(file_name,actual_image);
}

