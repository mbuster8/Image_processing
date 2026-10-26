#include "my_main_qlabel.h"

my_main_qlabel::my_main_qlabel(QWidget *parent) :QLabel(parent){
    scaleFactor_x = 1;
    scalefactor_y = 1;
}

void my_main_qlabel::mouseMoveEvent(QMouseEvent *ev){
    emit Mouse_position(ev->x(),ev->y());
}

void my_main_qlabel::wheelEvent(QWheelEvent *event){
    double numSteps = (event->delta() / 8) /15.0f;
    zoom(numSteps,event->x(),event->y());
}

void my_main_qlabel::zoom(double zoomfactor, int mouse_pos_x, int mouse_pos_y){

    //we will not zoom out to make picture disappear, there is no point to do that
    if (zoomfactor < 0 && zoom_of_picture <=1) return;


    if (zoomfactor>0)zoomfactor = 1.25;
    else zoomfactor = 1 / 1.25;
    zoom_of_picture *= zoomfactor;

    if(zoom_of_picture < 1) zoom_of_picture = 1;

    //mouse position depends on the displaying label size and size of the area
    // we want to display, so there scalefactor of pixmap comes together woth offset
    mouse_pos_x /= scaleFactor_x;
    mouse_pos_y /= scalefactor_y;
    mouse_pos_x +=offset_x;
    mouse_pos_y +=offset_y;

    //we need to maintain an area's to display width and height
    // first we calculate size of the area to be displayed
    int range_x = (int) ((double )original_pixmap.width() / zoom_of_picture);
    int range_y = (int) ((double )original_pixmap.height() / zoom_of_picture);

    if (range_y < 3 || range_x < 3){
        zoom_of_picture *=(1 / 1.25); //we need to set the zoom back
        return; //we are not going to zoom any more, it is not necessary
    }

    //now we try to fit the edges of the area to original pixmap
    int top_left_corner_x = mouse_pos_x - range_x/2;
    int top_left_corner_y = mouse_pos_y - range_y/2;
    int bottom_right_corner_x = mouse_pos_x + range_x/2;
    int bottom_right_corner_y = mouse_pos_y + range_y/2;

    // if the edges are out of the range, we need to set them back
    // so they fit into original pixmap
    if (top_left_corner_y < 0 ) {
        top_left_corner_y = 0;
        bottom_right_corner_y = range_y;
    }
    if (top_left_corner_x < 0 ) {
        top_left_corner_x = 0;
        bottom_right_corner_x = range_x;
    }
    if (bottom_right_corner_y > original_pixmap.height()) {
        bottom_right_corner_y = original_pixmap.height();
        top_left_corner_y = original_pixmap.height() - range_y;
    }
    if (bottom_right_corner_x > original_pixmap.width() ){
        bottom_right_corner_x = original_pixmap.width();
        top_left_corner_x = original_pixmap.width() - range_x;
    }

    //we need to set new scalefactor as precize as possible
    scaleFactor_x = (double) this->parentWidget()->width() / (double) (bottom_right_corner_x - top_left_corner_x);
    scalefactor_y = (double) this->parentWidget()->height() / (double) (bottom_right_corner_y - top_left_corner_y);

    //new pixmap offset depends on the top left corner of the rectangle we are going to display
    offset_x = top_left_corner_x;
    offset_y = top_left_corner_y;

    QPixmap pixmap_new = this->original_pixmap.copy(top_left_corner_x,
            top_left_corner_y,bottom_right_corner_x - top_left_corner_x,
            bottom_right_corner_y - top_left_corner_y);

    this->setPixmap(pixmap_new.scaled(this->parentWidget()->width(),this->parentWidget()->height(),Qt::KeepAspectRatio));
    this->setGeometry(0,0,this->pixmap()->width(),this->pixmap()->height());
}
