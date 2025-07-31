#include <iostream>
#include <thread>
#include <chrono>
#include <zmq.hpp>

int main() {
    std::cout << "Testing basic ZMQ REP/REQ pattern..." << std::endl;
    
    zmq::context_t context(1);
    
    // Server thread
    std::thread server_thread([&context]() {
        zmq::socket_t server(context, ZMQ_REP);
        server.bind("tcp://*:5557");
        std::cout << "Server: Bound to tcp://*:5557" << std::endl;
        
        zmq::message_t request;
        if (server.recv(request)) {
            std::string req_str(static_cast<char*>(request.data()), request.size());
            std::cout << "Server: Received " << req_str << std::endl;
            
            std::string reply = "OK";
            zmq::message_t response(reply.size());
            memcpy(response.data(), reply.data(), reply.size());
            server.send(response, zmq::send_flags::none);
            std::cout << "Server: Sent response" << std::endl;
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Client
    zmq::socket_t client(context, ZMQ_REQ);
    client.connect("tcp://localhost:5557");
    std::cout << "Client: Connected to tcp://localhost:5557" << std::endl;
    
    std::string msg = "Hello";
    zmq::message_t request(msg.size());
    memcpy(request.data(), msg.data(), msg.size());
    client.send(request, zmq::send_flags::none);
    std::cout << "Client: Sent " << msg << std::endl;
    
    zmq::message_t reply;
    if (client.recv(reply)) {
        std::string rep_str(static_cast<char*>(reply.data()), reply.size());
        std::cout << "Client: Received " << rep_str << std::endl;
    }
    
    server_thread.join();
    
    return 0;
}