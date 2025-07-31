#include <iostream>
#include <fstream>
#include "ZMQTransport.hpp"
#include "nlohmann/json.hpp"

using namespace DELILA::Net;
using json = nlohmann::json;

int main() {
    // Example 1: Configure from JSON object
    ZMQTransport transport1;
    
    json config = {
        {"transport", {
            {"data_channel", {
                {"address", "tcp://localhost:5555"},
                {"pattern", "PUB"},
                {"bind", true}
            }},
            {"status_channel", {
                {"address", "tcp://localhost:5556"},
                {"bind", true}
            }},
            {"command_channel", {
                {"address", "tcp://localhost:5557"},
                {"bind", false}
            }}
        }}
    };
    
    if (transport1.ConfigureFromJSON(config)) {
        std::cout << "Configured from JSON object successfully" << std::endl;
    }
    
    // Example 2: Configure from file
    ZMQTransport transport2;
    
    // Create example config file
    std::ofstream config_file("example_config.json");
    config_file << config.dump(4);
    config_file.close();
    
    if (transport2.ConfigureFromFile("example_config.json")) {
        std::cout << "Configured from file successfully" << std::endl;
    }
    
    // Example 3: DAQ role-based configuration
    json daq_config = {
        {"daq_role", "frontend"},
        {"component_id", "digitizer_01"},
        {"endpoints", {
            {"data_sink", "tcp://daq-server:5555"},
            {"control", "tcp://daq-server:5557"}
        }}
    };
    
    ZMQTransport transport3;
    if (transport3.ConfigureFromJSON(daq_config)) {
        std::cout << "Configured with DAQ role successfully" << std::endl;
    }
    
    return 0;
}