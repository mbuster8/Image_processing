#ifndef VISUAL_DEBUG_H
#define VISUAL_DEBUG_H

#ifndef VISUAL_DEBUG_GLOBAL_H
#define VISUAL_DEBUG_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(VISUAL_DEBUG_LIBRARY)
#  define VISUAL_DEBUGSHARED_EXPORT Q_DECL_EXPORT
#else
#  define VISUAL_DEBUGSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // VISUAL_DEBUG_GLOBAL_H

enum TYP_UDAJOV {
    TYP_IMAGE = 1
};

#include <zmq.hpp>
#include <opencv2/core/core.hpp>
#include <iostream>

VISUAL_DEBUGSHARED_EXPORT void send_image(cv::Mat image_to_send, std::string picture_name);
void fill_vector(std::vector<unsigned char> &my_vector,std::string input);
void intToBytes(std::vector<unsigned char> &my_vector, int my_int);

#endif // VISUAL_DEBUG_H
