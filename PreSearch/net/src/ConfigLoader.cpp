#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <cctype>
#include <cmath>

// Simple JSON parser implementation
// In a real implementation, you would use a proper JSON library like nlohmann/json

namespace delila {
namespace test {

// Simple JSON value class for parsing
class JsonValue {
public:
    enum Type { OBJECT, ARRAY, STRING, NUMBER, BOOLEAN, NULL_VALUE };
    
    Type type;
    std::string stringValue;
    double numberValue;
    bool boolValue;
    std::map<std::string, JsonValue> objectValue;
    std::vector<JsonValue> arrayValue;
    
    JsonValue() : type(NULL_VALUE), numberValue(0), boolValue(false) {}
    explicit JsonValue(const std::string& s) : type(STRING), stringValue(s), numberValue(0), boolValue(false) {}
    explicit JsonValue(double d) : type(NUMBER), numberValue(d), boolValue(false) {}
    explicit JsonValue(bool b) : type(BOOLEAN), numberValue(0), boolValue(b) {}
    
    bool HasMember(const std::string& key) const {
        return type == OBJECT && objectValue.find(key) != objectValue.end();
    }
    
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue nullValue;
        if (type == OBJECT) {
            auto it = objectValue.find(key);
            if (it != objectValue.end()) {
                return it->second;
            }
        }
        return nullValue;
    }
    
    JsonValue& operator[](const std::string& key) {
        type = OBJECT;
        return objectValue[key];
    }
    
    size_t Size() const {
        if (type == ARRAY) return arrayValue.size();
        if (type == OBJECT) return objectValue.size();
        return 0;
    }
    
    const JsonValue& operator[](size_t index) const {
        static JsonValue nullValue;
        if (type == ARRAY && index < arrayValue.size()) {
            return arrayValue[index];
        }
        return nullValue;
    }
};

// Simple JSON parser
class JsonParser {
public:
    static JsonValue Parse(const std::string& json) {
        JsonParser parser(json);
        return parser.ParseValue();
    }
    
private:
    std::string json;
    size_t pos;
    
    JsonParser(const std::string& j) : json(j), pos(0) {}
    
    void SkipWhitespace() {
        while (pos < json.length() && std::isspace(json[pos])) {
            ++pos;
        }
    }
    
    JsonValue ParseValue() {
        SkipWhitespace();
        if (pos >= json.length()) return JsonValue();
        
        char c = json[pos];
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseString();
        if (c == 't' || c == 'f') return ParseBoolean();
        if (c == 'n') return ParseNull();
        if (std::isdigit(c) || c == '-') return ParseNumber();
        
        return JsonValue();
    }
    
    JsonValue ParseObject() {
        JsonValue obj;
        obj.type = JsonValue::OBJECT;
        
        ++pos; // skip '{'
        SkipWhitespace();
        
        if (pos < json.length() && json[pos] == '}') {
            ++pos;
            return obj;
        }
        
        while (pos < json.length()) {
            SkipWhitespace();
            
            // Parse key
            if (json[pos] != '"') break;
            JsonValue key = ParseString();
            
            SkipWhitespace();
            if (pos >= json.length() || json[pos] != ':') break;
            ++pos; // skip ':'
            
            // Parse value
            JsonValue value = ParseValue();
            obj.objectValue[key.stringValue] = value;
            
            SkipWhitespace();
            if (pos >= json.length()) break;
            
            if (json[pos] == '}') {
                ++pos;
                break;
            }
            if (json[pos] == ',') {
                ++pos;
                continue;
            }
            break;
        }
        
        return obj;
    }
    
    JsonValue ParseArray() {
        JsonValue arr;
        arr.type = JsonValue::ARRAY;
        
        ++pos; // skip '['
        SkipWhitespace();
        
        if (pos < json.length() && json[pos] == ']') {
            ++pos;
            return arr;
        }
        
        while (pos < json.length()) {
            JsonValue value = ParseValue();
            arr.arrayValue.push_back(value);
            
            SkipWhitespace();
            if (pos >= json.length()) break;
            
            if (json[pos] == ']') {
                ++pos;
                break;
            }
            if (json[pos] == ',') {
                ++pos;
                continue;
            }
            break;
        }
        
        return arr;
    }
    
    JsonValue ParseString() {
        ++pos; // skip '"'
        std::string str;
        
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.length()) {
                ++pos;
                switch (json[pos]) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    case '\\': str += '\\'; break;
                    case '"': str += '"'; break;
                    default: str += json[pos]; break;
                }
            } else {
                str += json[pos];
            }
            ++pos;
        }
        
        if (pos < json.length()) ++pos; // skip closing '"'
        return JsonValue(str);
    }
    
    JsonValue ParseNumber() {
        std::string numStr;
        
        while (pos < json.length() && 
               (std::isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E')) {
            numStr += json[pos];
            ++pos;
        }
        
        return JsonValue(std::stod(numStr));
    }
    
    JsonValue ParseBoolean() {
        if (json.substr(pos, 4) == "true") {
            pos += 4;
            return JsonValue(true);
        } else if (json.substr(pos, 5) == "false") {
            pos += 5;
            return JsonValue(false);
        }
        return JsonValue();
    }
    
    JsonValue ParseNull() {
        if (json.substr(pos, 4) == "null") {
            pos += 4;
        }
        return JsonValue();
    }
};

// Config implementation
Config::Config() = default;

bool Config::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonContent = buffer.str();
    
    try {
        JsonValue root = JsonParser::Parse(jsonContent);
        return ParseJsonValue(root);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config file: " << e.what() << std::endl;
        return false;
    }
}

bool Config::SaveToFile(const std::string& filename) const {
    // Simple JSON serialization
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"test\": {\n";
    file << "    \"protocol\": \"" << GetProtocolString() << "\",\n";
    file << "    \"transport\": \"" << GetTransportString() << "\",\n";
    file << "    \"duration_minutes\": " << testConfig.durationMinutes << ",\n";
    file << "    \"batch_sizes\": [";
    for (size_t i = 0; i < testConfig.batchSizes.size(); ++i) {
        if (i > 0) file << ", ";
        file << testConfig.batchSizes[i];
    }
    file << "],\n";
    file << "    \"output_dir\": \"" << testConfig.outputDir << "\"\n";
    file << "  },\n";
    
    file << "  \"network\": {\n";
    file << "    \"merger_address\": \"" << networkConfig.mergerAddress << "\",\n";
    file << "    \"source1_port\": " << networkConfig.source1Port << ",\n";
    file << "    \"source2_port\": " << networkConfig.source2Port << ",\n";
    file << "    \"sink1_port\": " << networkConfig.sink1Port << ",\n";
    file << "    \"sink2_port\": " << networkConfig.sink2Port << "\n";
    file << "  },\n";
    
    file << "  \"grpc\": {\n";
    file << "    \"max_message_size\": " << grpcConfig.maxMessageSize << ",\n";
    file << "    \"keepalive_time_ms\": " << grpcConfig.keepaliveTimeMs << "\n";
    file << "  },\n";
    
    file << "  \"zeromq\": {\n";
    file << "    \"high_water_mark\": " << zmqConfig.highWaterMark << ",\n";
    file << "    \"linger_ms\": " << zmqConfig.lingerMs << ",\n";
    file << "    \"rcv_buffer_size\": " << zmqConfig.rcvBufferSize << "\n";
    file << "  },\n";
    
    file << "  \"logging\": {\n";
    file << "    \"level\": \"" << loggingConfig.level << "\",\n";
    file << "    \"directory\": \"" << loggingConfig.directory << "\"\n";
    file << "  }\n";
    file << "}\n";
    
    return true;
}

bool Config::ParseJsonValue(const JsonValue& root) {
    if (root.type != JsonValue::OBJECT) {
        return false;
    }
    
    if (root.HasMember("test")) {
        if (!ParseTestConfig(root["test"])) return false;
    }
    
    if (root.HasMember("network")) {
        if (!ParseNetworkConfig(root["network"])) return false;
    }
    
    if (root.HasMember("grpc")) {
        if (!ParseGrpcConfig(root["grpc"])) return false;
    }
    
    if (root.HasMember("zeromq")) {
        if (!ParseZeroMqConfig(root["zeromq"])) return false;
    }
    
    if (root.HasMember("logging")) {
        if (!ParseLoggingConfig(root["logging"])) return false;
    }
    
    return true;
}

bool Config::ParseTestConfig(const JsonValue& value) {
    if (value.type != JsonValue::OBJECT) return false;
    
    if (value.HasMember("protocol")) {
        testConfig.protocol = StringToTransportType(value["protocol"].stringValue);
    }
    
    if (value.HasMember("transport")) {
        testConfig.transport = StringToNetworkType(value["transport"].stringValue);
    }
    
    if (value.HasMember("duration_minutes")) {
        testConfig.durationMinutes = static_cast<uint32_t>(value["duration_minutes"].numberValue);
    }
    
    if (value.HasMember("batch_sizes")) {
        const JsonValue& batchSizes = value["batch_sizes"];
        if (batchSizes.type == JsonValue::ARRAY) {
            testConfig.batchSizes.clear();
            for (size_t i = 0; i < batchSizes.Size(); ++i) {
                testConfig.batchSizes.push_back(static_cast<uint32_t>(batchSizes[i].numberValue));
            }
        }
    }
    
    if (value.HasMember("output_dir")) {
        testConfig.outputDir = value["output_dir"].stringValue;
    }
    
    return true;
}

bool Config::ParseNetworkConfig(const JsonValue& value) {
    if (value.type != JsonValue::OBJECT) return false;
    
    if (value.HasMember("merger_address")) {
        networkConfig.mergerAddress = value["merger_address"].stringValue;
    }
    
    if (value.HasMember("hub_pub_port")) {
        networkConfig.hubPubPort = static_cast<uint16_t>(value["hub_pub_port"].numberValue);
    }
    
    if (value.HasMember("source1_port")) {
        networkConfig.source1Port = static_cast<uint16_t>(value["source1_port"].numberValue);
    }
    
    if (value.HasMember("source2_port")) {
        networkConfig.source2Port = static_cast<uint16_t>(value["source2_port"].numberValue);
    }
    
    if (value.HasMember("sink1_port")) {
        networkConfig.sink1Port = static_cast<uint16_t>(value["sink1_port"].numberValue);
    }
    
    if (value.HasMember("sink2_port")) {
        networkConfig.sink2Port = static_cast<uint16_t>(value["sink2_port"].numberValue);
    }
    
    return true;
}

bool Config::ParseGrpcConfig(const JsonValue& value) {
    if (value.type != JsonValue::OBJECT) return false;
    
    if (value.HasMember("max_message_size")) {
        grpcConfig.maxMessageSize = static_cast<uint32_t>(value["max_message_size"].numberValue);
    }
    
    if (value.HasMember("keepalive_time_ms")) {
        grpcConfig.keepaliveTimeMs = static_cast<uint32_t>(value["keepalive_time_ms"].numberValue);
    }
    
    return true;
}

bool Config::ParseZeroMqConfig(const JsonValue& value) {
    if (value.type != JsonValue::OBJECT) return false;
    
    if (value.HasMember("high_water_mark")) {
        zmqConfig.highWaterMark = static_cast<uint32_t>(value["high_water_mark"].numberValue);
    }
    
    if (value.HasMember("linger_ms")) {
        zmqConfig.lingerMs = static_cast<uint32_t>(value["linger_ms"].numberValue);
    }
    
    if (value.HasMember("rcv_buffer_size")) {
        zmqConfig.rcvBufferSize = static_cast<uint32_t>(value["rcv_buffer_size"].numberValue);
    }
    
    return true;
}

bool Config::ParseLoggingConfig(const JsonValue& value) {
    if (value.type != JsonValue::OBJECT) return false;
    
    if (value.HasMember("level")) {
        loggingConfig.level = value["level"].stringValue;
    }
    
    if (value.HasMember("directory")) {
        loggingConfig.directory = value["directory"].stringValue;
    }
    
    return true;
}

bool Config::IsValid() const {
    return !testConfig.batchSizes.empty() && 
           testConfig.durationMinutes > 0 &&
           !networkConfig.mergerAddress.empty() &&
           !testConfig.outputDir.empty() &&
           !loggingConfig.directory.empty();
}

std::string Config::GetProtocolString() const {
    return TransportTypeToString(testConfig.protocol);
}

std::string Config::GetTransportString() const {
    return NetworkTypeToString(testConfig.transport);
}

std::string Config::GetLogFilePath(ComponentType component) const {
    return loggingConfig.directory + "/" + ComponentTypeToString(component) + ".log";
}

std::string Config::GetResultsFilePath(const std::string& filename) const {
    return testConfig.outputDir + "/" + filename;
}

}} // namespace delila::test