#include "JsonWriter.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <iostream>
#include <sstream>

namespace uboot {

bool JsonWriter::Write(
    const std::string& outputPath,
    const std::vector<Entry>& entries,
    const std::vector<CollectorError>& errors,
    bool prettyPrint,
    const std::string& schemaVersion
) {
    std::string json = ToJson(entries, errors, prettyPrint, schemaVersion);
    
    if (outputPath.empty()) {
        // Write to stdout
        std::cout << json << std::endl;
        return true;
    }
    
    // Write to file with UTF-8 encoding
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write UTF-8 BOM
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    file.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    file << json;
    file.close();
    
    return true;
}

std::string JsonWriter::ToJson(
    const std::vector<Entry>& entries,
    const std::vector<CollectorError>& errors,
    bool prettyPrint,
    const std::string& schemaVersion
) {
    using namespace rapidjson;
    
    Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    
    // Add fields in stable order for deterministic output
    doc.AddMember("schemaVersion", Value(schemaVersion.c_str(), allocator), allocator);
    
    // Add timestamp (ISO 8601 format)
    auto now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    doc.AddMember("timestamp", Value(timestamp, allocator), allocator);
    
    // Add entries array
    Value entriesArray(kArrayType);
    for (const auto& entry : entries) {
        Value entryObj(kObjectType);
        
        // Add properties in stable order
        entryObj.AddMember("source", Value(entry.source.c_str(), allocator), allocator);
        entryObj.AddMember("scope", Value(entry.scope.c_str(), allocator), allocator);
        entryObj.AddMember("key", Value(entry.key.c_str(), allocator), allocator);
        entryObj.AddMember("location", Value(entry.location.c_str(), allocator), allocator);
        entryObj.AddMember("arguments", Value(entry.arguments.c_str(), allocator), allocator);
        entryObj.AddMember("imagePath", Value(entry.imagePath.c_str(), allocator), allocator);
        
        // Optional fields
        if (entry.keyName.has_value()) {
            entryObj.AddMember("keyName", Value(entry.keyName->c_str(), allocator), allocator);
        }
        if (entry.displayName.has_value()) {
            entryObj.AddMember("displayName", Value(entry.displayName->c_str(), allocator), allocator);
        }
        if (entry.publisher.has_value()) {
            entryObj.AddMember("publisher", Value(entry.publisher->c_str(), allocator), allocator);
        }
        if (entry.description.has_value()) {
            entryObj.AddMember("description", Value(entry.description->c_str(), allocator), allocator);
        }
        
        entriesArray.PushBack(entryObj, allocator);
    }
    doc.AddMember("entries", entriesArray, allocator);
    
    // Add errors array
    Value errorsArray(kArrayType);
    for (const auto& error : errors) {
        Value errorObj(kObjectType);
        errorObj.AddMember("source", Value(error.source.c_str(), allocator), allocator);
        errorObj.AddMember("message", Value(error.message.c_str(), allocator), allocator);
        if (error.errorCode != 0) {
            errorObj.AddMember("errorCode", error.errorCode, allocator);
        }
        errorsArray.PushBack(errorObj, allocator);
    }
    doc.AddMember("errors", errorsArray, allocator);
    
    // Serialize to string
    StringBuffer buffer;
    if (prettyPrint) {
        PrettyWriter<StringBuffer> writer(buffer);
        writer.SetIndent(' ', 2);
        doc.Accept(writer);
    } else {
        Writer<StringBuffer> writer(buffer);
        doc.Accept(writer);
    }
    
    return buffer.GetString();
}

std::string JsonWriter::EscapeJson(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (c < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

} // namespace uboot
