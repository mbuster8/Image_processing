#include "My_label.h"

My_label::My_label(int picture_number, QWidget *parent):QLabel(parent){
    set_identity(picture_number);
}

My_label::~My_label(){
}

void My_label::mousePressEvent(QMouseEvent *){
    emit clicked();
}

int My_label::get_identity(){
    return identity;
}

void My_label::set_identity(int picture_number){
    identity = picture_number;
}

void My_label::reload_picture(QImage new_image, int actual_picture_ID){
    if (this->get_identity() == actual_picture_ID) this->setPixmap(QPixmap::fromImage(new_image).scaled(140,140,Qt::KeepAspectRatio));
}
