#include "ConfigLoader.h"

bool ConfigLoader::isInteger(const std::string& str, int& result) const {
    char* p;
    long value = strtol(str.c_str(), &p, 10);
    if (*p == 0) { 
        result = static_cast<int>(value); 
        return true;
    }
    return false;
}

bool ConfigLoader::isDouble(const std::string& str, double& result) const {
    char* p;

    // std::string normalized = str;
    // std::replace(normalized.begin(), normalized.end(), ',', '.'); // Replace ',' with '.'
    // double value = strtod(normalized.c_str(), &p);

    double value = strtod(str.c_str(), &p);
    if (*p == 0) { 
        result = value;
        return true;
    }
    return false;
}

bool ConfigLoader::isBoolean(const std::string& str, bool& result) const {
    if (str == "true" || str == "TRUE" || str == "True") {
        result = true;
        return true;
    }
    if (str == "false" || str == "FALSE" || str == "False") {
        result = false;
        return true;
    }
    return false;
}

bool ConfigLoader::isQuotedString(const std::string& str, std::string& result) const {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        result = str.substr(1, str.size() - 2); // Remove  ""
        return true;
    }
    return false;
}

void ConfigLoader::processLine(const std::string& line) {
    // Process comments
    std::string lineWithoutComment = line;
    size_t commentPos = line.find('#');
    if (commentPos != std::string::npos) {
        lineWithoutComment = line.substr(0, commentPos);
    }

    // Eliminate blank spaces
    lineWithoutComment.erase(0, lineWithoutComment.find_first_not_of(" \t"));
    lineWithoutComment.erase(lineWithoutComment.find_last_not_of(" \t") + 1);

    // Do it has data?
    if (lineWithoutComment.empty()) return;

    size_t pos = lineWithoutComment.find('=');
    if (pos == std::string::npos) return; // Need it =

    std::string key = lineWithoutComment.substr(0, pos);
    std::string value = lineWithoutComment.substr(pos + 1);

    // Trim whitespace from key and value
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    // Determine value type (integer, double, boolean, string)
    int intResult;
    double doubleResult;
    bool boolResult;
    std::string stringResult;

    if (isInteger(value, intResult)) {
        configData[key] = intResult;
    } else if (isDouble(value, doubleResult)) {
        configData[key] = doubleResult;
    } else if (isBoolean(value, boolResult)) {
        configData[key] = boolResult;
    } else if (isQuotedString(value, stringResult)) {
        configData[key] = stringResult;
    } else {
        throw std::runtime_error(
            "Key \"" + key + "\" with value \"" + value + "\" type not recognized.\n" + TypeExample
        );
    }
}

void ConfigLoader::loadTxt(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open text file: " + filename);
    }

    std::string line;
    while (getline(file, line)) {
        // Skip empty lines or comments
        if (line.empty() || line[0] == '#') continue;
        processLine(line);
    }
}

void ConfigLoader::loadToml(const std::string& filename) {
    auto config = toml::parse_file(filename);
    processTomlTable(config);
}

ConfigTypes ConfigLoader::processSimpleValue(const toml::node& value) {
    if (value.is_integer()) {
        return static_cast<int>(value.as_integer()->get());
    } else if (value.is_floating_point()) {
        return value.as_floating_point()->get();
    } else if (value.is_string()) {
        return value.as_string()->get();
    } else if (value.is_boolean()) {
        return value.as_boolean()->get();
    } else {
        throw std::runtime_error("Unsupported TOML value encountered.");
    }
}

ConfigTypes ConfigLoader::processArray(const toml::array& array) {
    if (array.empty()) {
        throw std::runtime_error("Empty arrays are not supported.");
    }

    if (array.front().is_integer()) {
        std::vector<int> result;
        for (const auto& element : array) {
            if (element.is_integer()) {
                result.push_back(element.as_integer()->get());
            } else {
                throw std::runtime_error("Mixed-type arrays are not supported.");
            }
        }
        return result;
    } else if (array.front().is_floating_point()) {
        std::vector<double> result;
        for (const auto& element : array) {
            if (element.is_floating_point()) {
                result.push_back(element.as_floating_point()->get());
            } else {
                throw std::runtime_error("Mixed-type arrays are not supported.");
            }
        }
        return result;
    } else if (array.front().is_string()) {
        std::vector<std::string> result;
        for (const auto& element : array) {
            if (element.is_string()) {
                result.push_back(element.as_string()->get());
            } else {
                throw std::runtime_error("Mixed-type arrays are not supported.");
            }
        }
        return result;
    } else if (array.front().is_boolean()) {
        std::vector<bool> result;
        for (const auto& element : array) {
            if (element.is_boolean()) {
                result.push_back(element.as_boolean()->get());
            } else {
                throw std::runtime_error("Mixed-type arrays are not supported.");
            }
        }
        return result;
    } else {
        throw std::runtime_error("Unsupported array type encountered.");
    }
}

void ConfigLoader::processTomlTable(const toml::table& table, const std::string& prefix) {
    for (const auto& [key, value] : table) {
        std::string fullKey = prefix.empty() ? std::string(key.str()) : prefix + "." + std::string(key.str());

        if (value.is_table()) {
            // Process tables recursively
            processTomlTable(*value.as_table(), fullKey);
        } else if (value.is_array()) {
            // Process arrays and store them as vector<T>.
            configData[fullKey] = processArray(*value.as_array());
        } else {
            // Process simple values
            configData[fullKey] = processSimpleValue(value);
        }
    }
}

void ConfigLoader::load(const std::string& filename) {
    // Save the current global locale and set the locale to "en_US.UTF-8"
    std::locale originalLocale = std::locale::global(std::locale("en_US.UTF-8"));
    
    // Check the file extension and load the appropriate format
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".toml") {
        loadToml(filename);  // Load the TOML file if it has a .toml extension
    } else {
        loadTxt(filename);   // Otherwise, load the file as a text file
    }
    
    // Restore the original global locale to ensure no side effects on the rest of the program
    std::locale::global(originalLocale);
    }


void ConfigLoader::printConfig() const {
        for (const auto& [key, value] : configData) {
            std::cout << key << " = ";
            std::visit([&](auto&& v) { 
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::vector<int>>) {
                    std::cout << vectorToString(v);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::vector<double>>) {
                    std::cout << vectorToString(v);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::vector<std::string>>) {
                    std::cout << vectorToString(v);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::vector<bool>>) {
                    std::cout << vectorToString(v);
                } else {
                    std::cout << v;
                }
            }, value);
            std::cout << std::endl;
        }
    }

std::string ConfigLoader::getTypeName(const ConfigTypes& value) {
    return std::visit([](const auto& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int>) return "int";
        else if constexpr (std::is_same_v<T, double>) return "double";
        else if constexpr (std::is_same_v<T, std::string>) return "std::string";
        else if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, std::vector<int>>) return "std::vector<int>";
        else if constexpr (std::is_same_v<T, std::vector<double>>) return "std::vector<double>";
        else if constexpr (std::is_same_v<T, std::vector<std::string>>) return "std::vector<std::string>";
        else if constexpr (std::is_same_v<T, std::vector<bool>>) return "std::vector<bool>";
        else return "unknown";
    }, value);
}

bool ConfigLoader::exists(const std::string& key) const {
    return configData.contains(key);
}

std::vector<std::string_view> ConfigLoader::getKeys() const {
    auto keys_view = std::views::keys(configData);
    return std::vector<std::string_view>(keys_view.begin(), keys_view.end());
}

std::vector<std::string_view> ConfigLoader::getSurNames(const std::string& key) const {
    auto keys_view = std::views::keys(configData) 
        | std::views::filter([&](std::string_view k) { 
            return k.contains(key); // C++23 contains es más limpio
        })
        | std::views::transform([&](std::string_view k) -> std::string_view {
            // Buscamos el primer y segundo punto
            size_t first_dot = k.find('.');
            size_t second_dot = k.find('.', first_dot + 1);
            
            if (first_dot != std::string_view::npos && second_dot != std::string_view::npos) {
                return k.substr(first_dot + 1, second_dot - first_dot - 1);
            }
            return ""; // O el manejo que prefieras si el formato falla
        })
        | std::views::filter([](std::string_view s) { return !s.empty(); });

    // Pasamos a vector
    std::vector<std::string_view> result(keys_view.begin(), keys_view.end());

    // Eliminamos duplicados (episodic saldría 2 veces)
    std::ranges::sort(result);
    auto [ret, _] = std::ranges::unique(result);
    result.erase(ret, result.end());

    return result;
}
    
