
#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <ranges>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <toml++/toml.h>


// Alias for configuration types
using ConfigTypes = std::variant<int, double, std::string, bool, 
                                 std::vector<int>, 
                                 std::vector<double>, 
                                 std::vector<std::string>, 
                                 std::vector<bool>>;

const std::string TypeExample = 
    "Examples of types:\n"
    "Basic Types\n"
    "- int = 123\n"
    "- double = 123.45\n"
    "- bool = True\n"
    "- std::string = \"example\"\n"
    "If using .Toml:\n"
    "- std::vector<int> = [1, 2, 3]\n"
    "- std::vector<double> = [1.1, 2.2, 3.3]\n"
    "- std::vector<std::string> = [\"a\", \"b\", \"c\"]\n"
    "- std::vector<bool> = [true, false, true]\n"
    "<You can comment with #>\n";

/**
 * @class ConfigLoader
 * @brief A class to load and manage configuration data from TOML and text files.
 */
class ConfigLoader {
private:
    std::unordered_map<std::string, ConfigTypes> configData;

    // Helper functions for type detection
    bool isInteger(const std::string& str, int& result) const;
    bool isDouble(const std::string& str, double& result) const;
    bool isBoolean(const std::string& str, bool& result) const;
    bool isQuotedString(const std::string& str, std::string& result) const;

    // Helper to process a line from a text configuration file
    void processLine(const std::string& line);

    // Loaders for specific file types
    void loadTxt(const std::string& filename);
    void loadToml(const std::string& filename);

    ConfigTypes processArray(const toml::array& array);
    ConfigTypes processSimpleValue(const toml::node& value);

    template <typename T>
    static std::string vectorToString(const std::vector<T>& vec);

    // Recursive processing of TOML tables
    void processTomlTable(const toml::table& table, const std::string& prefix = "");

    static std::string getTypeName(const ConfigTypes& value);

public:
    /**
     * @brief Retrieves the value for a given key and casts it to the specified type.
     * @tparam T The expected type of the value.
     * @param key The configuration key.
     * @return The value cast to the specified type.
     * @throws std::runtime_error If the key is not found or the type does not match.
     */
    template <typename T>
    T get(const std::string& key) const;

    ConfigTypes get(const std::string& key) const;

    /**
     * @brief Loads a configuration file (TOML or text format).
     * @param filename The path to the configuration file.
     */
    void load(const std::string& filename);

    /**
     * @brief Prints all loaded configuration data to the console.
     */
    void printConfig() const;

    /**
     * @brief Checks if a key exists in the configuration.
     * @param key The key to check.
     * @return True if the key exists, false otherwise.
     */
    bool exists(const std::string& key) const;

    /**
     * @brief Retrieves all keys in the configuration.
     * @return A vector of string views containing all keys.
     */
    std::vector<std::string_view> getKeys() const;
    
    /**
     * @brief Retrieves all suffixes of a given key in the configuration.
     * @param key The key to check.
     * @return A vector of string views containing all suffixes of the key.
     */
    std::vector<std::string_view> getSurNames(const std::string& key) const;
};
#include "ConfigLoader.tpp"
#endif // CONFIG_LOADER_H
