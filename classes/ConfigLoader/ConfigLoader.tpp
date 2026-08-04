#ifndef CONFIG_LOADER_TPP
#define CONFIG_LOADER_TPP

#include "ConfigLoader.h"

#include <type_traits>   // std::is_same_v, for the int -> double widening below

template <typename T>
T ConfigLoader::get(const std::string& key) const {
    auto it = configData.find(key);
    if (it == configData.end()) {
        throw std::runtime_error("Key not found: " + key);
    }

    // ── int -> double WIDENING ────────────────────────────────────────────────────────────────────
    // Whether a TOML scalar lands in the variant as `int` or `double` depends only on whether it was
    // WRITTEN with a decimal point: `Period = 25` is an int, `Period = 25.0` a double. Both mean the
    // same number to the caller, so a get<double> on the first used to throw for a purely lexical
    // reason — and every agent's load_optional_cast swallows that exception, so the configured value
    // was silently replaced by the built-in default with nothing logged anywhere. (Found 2026-08-03:
    // controller's VelocityOutputPeriodMs = 25 and ControlPollMs = 10 had never taken effect.)
    // int -> double is exact for every value a config file can hold, so accept it.
    // Deliberately ONE-WAY: double -> int stays an error, since that one would silently truncate.
    if constexpr (std::is_same_v<T, double>) {
        if (const int* as_int = std::get_if<int>(&it->second))
            return static_cast<double>(*as_int);
    }
    // Same lexical trap one level up: `[1, 2, 3]` is a vector<int>, `[1.0, 2.0, 3.0]` a vector<double>.
    if constexpr (std::is_same_v<T, std::vector<double>>) {
        if (const std::vector<int>* as_ints = std::get_if<std::vector<int>>(&it->second))
            return std::vector<double>(as_ints->begin(), as_ints->end());
    }

    try {
        return std::get<T>(it->second);
    } catch (const std::bad_variant_access& e) {
        throw std::runtime_error("Key \"" + key + "\" type mismatch.\n"
        "Type identified: " + ConfigLoader::getTypeName(it->second) + "\n"
        "Requested type: " +
            ([] {
                // Mapeo de nombres de typeid a nombres legibles
                static const std::unordered_map<std::string, std::string> typeNames = {
                    {"i", "int"},
                    {"d", "double"},
                    {"b", "bool"},
                    {"NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE", "std::string"},
                    {"St3vectorIiE", "std::vector<int>"},
                    {"St3vectorIdE", "std::vector<double>"},
                    {"St3vectorIbE", "std::vector<bool>"},
                    {"St3vectorINSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE", "std::vector<std::string>"}
                };

                auto it = typeNames.find(typeid(T).name());
                return (it != typeNames.end()) ? it->second : std::string(typeid(T).name());
            })() +
            "\n" + TypeExample);
    }
}

    template <typename T>
    std::string ConfigLoader::vectorToString(const std::vector<T>& vec) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            oss << vec[i];
            if (i < vec.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]";
        return oss.str();
    }


#endif // CONFIG_LOADER_TPP

