#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QThread>
#include <opencv2/opencv.hpp>
#include <QMutex>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <zmq.hpp>
#include <iostream>

class MyThread : public QThread{
    Q_OBJECT

public:
    explicit MyThread(QObject *parent = 0);
    void run();
    void decodeData(const QByteArray &inputData);
    bool stop;

signals:
    void incoming_image(QString name, cv::Mat incoming_mat);

public slots:

};

#endif // MYTHREAD_H
