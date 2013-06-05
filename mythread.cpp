#include "mythread.h"

enum TYP_UDAJOV {
    TYP_IMAGE = 1
};

using namespace std;

MyThread::MyThread(QObject *parent) : QThread(parent){

}

void MyThread::run(){
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:7139");
    stop = false;

    while (true) {
        if (stop) break;
        zmq::message_t request;        

        // Wait for next request from client
        socket.recv (&request);
        std::cout << "Received data" << std::endl;

        QByteArray data((const char *) request.data(), request.size());
        decodeData(data);

        // Send reply back to client
        zmq::message_t reply (2);
        memcpy ((void *) reply.data (), "OK", 2);
        socket.send (reply);
    }
}

void MyThread::decodeData(const QByteArray &inputData){
    const int* header = (const int*) inputData.constData();
    int typ = header[0];
    int dlzkaNazvu = header[1];
    QString nazov = QString::fromUtf8((const char *) &header[2], dlzkaNazvu);


    if (typ == TYP_IMAGE) {
        const int *imgHeader = (const int *)((const char *)&header[2] + dlzkaNazvu);
        int sirka = imgHeader[0];
        int vyska = imgHeader[1];
        int flags = imgHeader[2];

        const void *data = &imgHeader[3];

        cv::Mat image(vyska, sirka, flags, (void*) data);
        cv::Mat result = image.clone();

        emit incoming_image(nazov, result);
    }
    else {
        std::cerr << "Unknown type: " << typ << std::endl;
    }
}

/*
    |||              Generic header               ||     Specific header     |   Data...  |||
    ||| type | picture_name_length | picture_name || width | height | flags  |   Data...  |||
 */
