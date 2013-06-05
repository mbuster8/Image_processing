#include "visual_debug.h"

void fill_vector(std::vector<unsigned char> &my_vector,std::string input){
    for (unsigned int i=0;i<input.length();i++) my_vector.push_back(input[i]);
}

void intToBytes(std::vector<unsigned char> &my_vector, int my_int){
     for(int i=0; i<4;i++) my_vector.push_back(my_int>>(i*8));
}

void send_image(cv::Mat image_to_send, std::string picture_name){

    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REQ);
    std::cout << "Connecting to visualizer..." << std::endl;
    socket.connect ("tcp://localhost:7139");

    std::vector <unsigned char> vector_output; //here we store message header
    int type = TYP_IMAGE;
    int length = picture_name.length();

    //filling vector with header information
    intToBytes(vector_output,type);
    intToBytes(vector_output,length);
    fill_vector(vector_output,picture_name);
    intToBytes(vector_output,image_to_send.cols);
    intToBytes(vector_output,image_to_send.rows);
    intToBytes(vector_output,image_to_send.flags);

    zmq::message_t zmq_message(vector_output.size() +
                   (image_to_send.dataend-image_to_send.datastart)); //new message of header+data size
    memcpy ((void *) zmq_message.data(),vector_output.data(),
            vector_output.size()); //copy header to the message
    memcpy((void *) zmq_message.data()+vector_output.size(), //copy data to the message
           image_to_send.data,image_to_send.dataend-image_to_send.datastart);

    std::cout << "Sending image..."<< std::endl;
    socket.send(zmq_message);

    zmq::message_t reply;
    socket.recv (&reply);
    std::cout << "OK"<< std::endl;
}
