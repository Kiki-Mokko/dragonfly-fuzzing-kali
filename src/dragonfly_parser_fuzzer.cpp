#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <sstream>

class DragonflyParserFuzzer {
public:
    int TestParserInterface(const std::vector<char>& resp_data) {
        if (resp_data.empty()) {
            return 0; // Пустые данные - код 0
        }
        
        int result_code = 1; // Начинаем с кода 1
        int command_count = 0;
        int total_length = resp_data.size();
        
        try {
            size_t pos = 0;
            
            while (pos < resp_data.size()) {
                if (pos >= resp_data.size()) break;
                
                char type_char = resp_data[pos++];
                result_code += (int)type_char; // Учитываем тип в результате
                
                switch (type_char) {
                    case '*': // Массив
                        command_count += ParseArray(resp_data, pos, result_code);
                        break;
                    case '$': // Bulk string
                        command_count += ParseBulkString(resp_data, pos, result_code);
                        break;
                    case '+': // Simple string
                        command_count += ParseSimpleString(resp_data, pos, result_code);
                        break;
                    case '-': // Error
                        ParseError(resp_data, pos, result_code);
                        break;
                    case ':': // Integer
                        ParseInteger(resp_data, pos, result_code);
                        break;
                    default:
                        result_code += 1000; // Неизвестный тип
                        pos = resp_data.size(); // Завершаем
                        break;
                }
                
                // Защита от бесконечного цикла
                if (pos > resp_data.size() * 2) break;
            }
            
        } catch (const std::exception& e) {
            result_code += 50000; // Исключения дают уникальный код
        } catch (...) {
            result_code += 100000; // Неизвестные исключения
        }
        
        // Создаём РАЗНЫЕ коды возврата на основе результатов парсинга
        int final_code = (result_code + command_count * 100 + total_length) % 256;
        return final_code;
    }

private:
    int ParseArray(const std::vector<char>& data, size_t& pos, int& result_code) {
        int elements_found = 0;
        std::string count_str;
        
        // Читаем количество элементов
        while (pos < data.size() && data[pos] != '\r') {
            count_str += data[pos++];
            result_code += 10;
        }
        
        if (pos >= data.size() || data[pos] != '\r') return 0;
        pos++; // \r
        if (pos >= data.size() || data[pos] != '\n') return 0;
        pos++; // \n
        
        try {
            int element_count = std::stoi(count_str);
            result_code += element_count * 100;
            
            // Парсим элементы
            for (int i = 0; i < element_count && pos < data.size(); i++) {
                if (pos >= data.size()) break;
                
                char elem_type = data[pos++];
                result_code += (int)elem_type * 5;
                
                switch (elem_type) {
                    case '$':
                        elements_found += ParseBulkString(data, pos, result_code);
                        break;
                    case '+':
                        elements_found += ParseSimpleString(data, pos, result_code);
                        break;
                    default:
                        result_code += 50;
                        break;
                }
            }
            return elements_found;
            
        } catch (...) {
            result_code += 1000;
            return 0;
        }
    }
    
    int ParseBulkString(const std::vector<char>& data, size_t& pos, int& result_code) {
        std::string length_str;
        int commands_found = 0;
        
        // Читаем длину
        while (pos < data.size() && data[pos] != '\r') {
            length_str += data[pos++];
            result_code += 2;
        }
        
        if (pos >= data.size() || data[pos] != '\r') return 0;
        pos++;
        if (pos >= data.size() || data[pos] != '\n') return 0;
        pos++;
        
        try {
            int str_length = std::stoi(length_str);
            result_code += str_length * 10;
            
            if (str_length == -1) {
                return 0; // Null bulk string
            }
            
            if (str_length < 0 || pos + str_length > data.size()) {
                result_code += 500;
                return 0;
            }
            
            // Извлекаем строку и проверяем команды
            std::string content;
            for (int i = 0; i < str_length && pos < data.size(); i++) {
                content += data[pos++];
            }
            
            // Проверяем является ли это командой
            if (ValidateCommand(content)) {
                commands_found++;
                result_code += content.length() * 100;
            }
            
            // Проверяем окончание
            if (pos + 1 >= data.size() || data[pos] != '\r' || data[pos + 1] != '\n') {
                result_code += 200;
                return commands_found;
            }
            pos += 2;
            
            return commands_found;
            
        } catch (...) {
            result_code += 1000;
            return 0;
        }
    }
    
    int ParseSimpleString(const std::vector<char>& data, size_t& pos, int& result_code) {
        std::string content;
        int commands_found = 0;
        
        while (pos < data.size() && data[pos] != '\r') {
            content += data[pos++];
            result_code += 3;
        }
        
        if (pos >= data.size() || data[pos] != '\r') return 0;
        pos++;
        if (pos >= data.size() || data[pos] != '\n') return 0;
        pos++;
        
        if (ValidateCommand(content)) {
            commands_found++;
            result_code += content.length() * 50;
        }
        
        return commands_found;
    }
    
    void ParseError(const std::vector<char>& data, size_t& pos, int& result_code) {
        while (pos < data.size() && data[pos] != '\r') {
            result_code += 7;
            pos++;
        }
        
        if (pos < data.size() && data[pos] == '\r') pos++;
        if (pos < data.size() && data[pos] == '\n') pos++;
        
        result_code += 300;
    }
    
    void ParseInteger(const std::vector<char>& data, size_t& pos, int& result_code) {
        while (pos < data.size() && data[pos] != '\r') {
            result_code += 4;
            pos++;
        }
        
        if (pos < data.size() && data[pos] == '\r') pos++;
        if (pos < data.size() && data[pos] == '\n') pos++;
        
        result_code += 200;
    }
    
    bool ValidateCommand(const std::string& command) {
        static const std::vector<std::string> valid_commands = {
            "PING", "GET", "SET", "HSET", "HGET", "HDEL", 
            "LPUSH", "RPUSH", "LPOP", "RPOP", "DEL", "EXISTS",
            "INCR", "DECR", "EXPIRE", "TTL", "KEYS", "FLUSHDB",
            "QUIT", "SELECT", "AUTH"
        };
        
        for (const auto& valid_cmd : valid_commands) {
            if (command == valid_cmd) {
                return true;
            }
        }
        return false;
    }
};

int main(int argc, char* argv[]) {
    DragonflyParserFuzzer fuzzer;
    
    std::vector<char> input_data;
    char buffer[4096];
    
    // Чтение входных данных
    if (argc > 1) {
        // Чтение из файла (для AFL++)
        FILE* file = fopen(argv[1], "rb");
        if (file) {
            while (size_t count = fread(buffer, 1, sizeof(buffer), file)) {
                input_data.insert(input_data.end(), buffer, buffer + count);
            }
            fclose(file);
        }
    } else {
        // Чтение из stdin (для тестирования)
        while (std::cin.read(buffer, sizeof(buffer)) || std::cin.gcount()) {
            input_data.insert(input_data.end(), buffer, buffer + std::cin.gcount());
        }
    }
    
    return fuzzer.TestParserInterface(input_data);
}
