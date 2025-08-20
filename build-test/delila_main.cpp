// Main source file for DELILA library
#include <iostream>

namespace DELILA {
    const char* getVersion() {
        return "1.0.0";
    }
    
    bool initialize(const char* config_path = nullptr) {
        std::cout << "DELILA2 v" << getVersion() << " initialized" << std::endl;
        return true;
    }
    
    void shutdown() {
        std::cout << "DELILA2 shutdown complete" << std::endl;
    }
}
