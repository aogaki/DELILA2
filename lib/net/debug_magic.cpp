#include <iostream>
#include <cstdint>

int main() {
    constexpr uint64_t MAGIC_NUMBER = 0x44454C494C413200ULL; // "DELILA2\0"
    
    std::cout << "MAGIC_NUMBER: 0x" << std::hex << MAGIC_NUMBER << std::dec << std::endl;
    
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&MAGIC_NUMBER);
    
    std::cout << "Byte layout (little-endian):" << std::endl;
    for (int i = 0; i < 8; i++) {
        std::cout << "bytes[" << i << "]: 0x" << std::hex << (int)bytes[i] << " '" << (char)bytes[i] << "'" << std::dec << std::endl;
    }
    
    std::cout << "\nExpected string: DELILA2\\0" << std::endl;
    std::cout << "Expected hex: D(0x44) E(0x45) L(0x4C) I(0x49) L(0x4C) A(0x41) 2(0x32) \\0(0x00)" << std::endl;
    
    return 0;
}