// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/**
 * @file reg_param.h
 * @brief CCI parameter wrapper used by SEP models (replaces csml_param).
 *
 * New SMC-style models should declare cci::cci_param<T> directly. This wrapper
 * keeps the vector-as-JSON and INI-file preset helpers that existing SEP
 * models and testbenches already call.
 */

#pragma once

#include <cci_configuration>
#include <cci/utils/broker.h>

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <systemc>
#include <type_traits>
#include <vector>

namespace regmodel {

template <typename U>
struct is_std_vector : std::false_type {};

template <typename Elem, typename Alloc>
struct is_std_vector<std::vector<Elem, Alloc>> : std::true_type {};

inline std::map<std::string, std::string> parse_config_file(const std::string& cci_config_file) {
    std::string s, key, value;
    std::map<std::string, std::string> cci_params;
    std::ifstream f(cci_config_file);
    std::string val_type;
    while (std::getline(f, s)) {
        std::string::size_type begin = s.find_first_not_of(" \f\t\v");
        if (begin == std::string::npos) continue;
        if (std::string("#;//").find(s[begin]) != std::string::npos) continue;

        if (s[begin] == '@') {
            std::istringstream ss(s.substr(begin + 1));
            std::string directive, incfile;
            ss >> directive >> incfile;
            if (directive == "include" && !incfile.empty()) {
                auto slash = cci_config_file.find_last_of("/\\");
                if (slash != std::string::npos)
                    incfile = cci_config_file.substr(0, slash + 1) + incfile;
                std::ifstream probe(incfile);
                if (!probe.is_open())
                    std::cout << "[reg_param] WARNING: @include file not found: " << incfile
                              << std::endl;
                else {
                    probe.close();
                    auto sub = parse_config_file(incfile);
                    cci_params.insert(sub.begin(), sub.end());
                }
            }
            continue;
        }

        if (s[begin] == '[') {
            std::string::size_type last = s.find(']', begin + 1);
            if (last != std::string::npos && last > begin + 1) {
                val_type = s.substr(begin + 1, last - begin - 1);
                continue;
            }
        }

        std::string::size_type end = s.find(':', begin);
        key = s.substr(begin, end - begin);
        key.erase(key.find_last_not_of(" \f\t\r\v") + 1);

        if (key.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_.") !=
            std::string::npos) {
            std::cout << "Error parsing input cci parameter configuration file" << std::endl;
            cci_params.clear();
            return cci_params;
        }
        if (key.empty()) continue;

        begin = s.find_first_not_of(" \f\n\r\t\v", end + 1);
        end = s.find_last_not_of(" \f\n\r\t\v") + 1;
        value = s.substr(begin, end - begin);
        if (val_type == "string") {
            value = "\"" + value + "\"";
        }
        cci_params[key] = value;
    }
    return cci_params;
}

template <typename T>
class Param {
private:
    cci::cci_broker_handle m_broker;
    using stored_value_type =
        typename std::conditional<is_std_vector<T>::value, std::string, T>::type;
    cci::cci_param<stored_value_type> param_cci;

public:
    template <typename U>
    using is_std_vector = regmodel::is_std_vector<U>;

    static std::string trim_copy(const std::string& s) {
        std::size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
        std::size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
        return s.substr(b, e - b);
    }

    static std::vector<std::string> split_top_level_csv(const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        bool in_quotes = false;
        bool escape = false;
        for (char c : s) {
            if (escape) {
                cur.push_back(c);
                escape = false;
                continue;
            }
            if (c == '\\' && in_quotes) {
                escape = true;
                continue;
            }
            if (c == '"') {
                in_quotes = !in_quotes;
                cur.push_back(c);
                continue;
            }
            if (c == ',' && !in_quotes) {
                out.push_back(trim_copy(cur));
                cur.clear();
                continue;
            }
            cur.push_back(c);
        }
        if (!cur.empty() || !out.empty()) out.push_back(trim_copy(cur));
        return out;
    }

    static std::string unquote_json_string(const std::string& s) {
        std::string t = trim_copy(s);
        if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
            t = t.substr(1, t.size() - 2);
        }
        std::string out;
        out.reserve(t.size());
        bool escape = false;
        for (char c : t) {
            if (escape) {
                out.push_back(c);
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

    template <typename Elem>
    static std::string elem_to_json(const Elem& v) {
        if constexpr (std::is_same_v<Elem, std::string>) {
            std::string out = "\"";
            for (char c : v) {
                if (c == '\\' || c == '"') out.push_back('\\');
                out.push_back(c);
            }
            out += "\"";
            return out;
        } else if constexpr (std::is_same_v<Elem, bool>) {
            return v ? "true" : "false";
        } else {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }
    }

    template <typename Vec>
    static std::string vector_to_json_array(const Vec& vec) {
        std::string out = "[";
        for (std::size_t i = 0; i < vec.size(); ++i) {
            if (i != 0) out += ",";
            out += elem_to_json<typename Vec::value_type>(vec[i]);
        }
        out += "]";
        return out;
    }

    template <typename Elem>
    static Elem json_token_to_elem(const std::string& tok) {
        if constexpr (std::is_same_v<Elem, std::string>) {
            return unquote_json_string(tok);
        } else if constexpr (std::is_same_v<Elem, bool>) {
            const std::string t = trim_copy(tok);
            return (t == "true" || t == "1");
        } else {
            std::stringstream ss(trim_copy(tok));
            Elem v{};
            ss >> v;
            return v;
        }
    }

    template <typename Vec>
    static Vec json_array_to_vector(const std::string& json) {
        Vec out;
        std::string t = trim_copy(json);
        if (t.size() < 2 || t.front() != '[' || t.back() != ']') {
            if (!t.empty()) out.push_back(json_token_to_elem<typename Vec::value_type>(t));
            return out;
        }
        t = trim_copy(t.substr(1, t.size() - 2));
        if (t.empty()) return out;
        const auto tokens = split_top_level_csv(t);
        out.reserve(tokens.size());
        for (const auto& tok : tokens) {
            out.push_back(json_token_to_elem<typename Vec::value_type>(tok));
        }
        return out;
    }

    template <typename U = T,
              typename std::enable_if<!is_std_vector<U>::value &&
                                          !std::is_same<stored_value_type, std::string>::value,
                                      int>::type = 0>
    Param(const std::string& name)
        : m_broker(cci::cci_get_broker()), param_cci(name.c_str()) {}

    template <typename U = T,
              typename std::enable_if<!is_std_vector<U>::value &&
                                          !std::is_same<stored_value_type, std::string>::value,
                                      int>::type = 0>
    Param(const std::string& name, U default_value)
        : m_broker(cci::cci_get_broker()), param_cci(name.c_str(), default_value) {}

    template <typename U = T,
              typename std::enable_if<!is_std_vector<U>::value &&
                                          std::is_same<stored_value_type, std::string>::value,
                                      int>::type = 0>
    Param(const std::string& name)
        : m_broker(cci::cci_get_broker()), param_cci(name.c_str(), stored_value_type()) {}

    template <typename U = T,
              typename std::enable_if<!is_std_vector<U>::value &&
                                          std::is_same<stored_value_type, std::string>::value,
                                      int>::type = 0>
    Param(const std::string& name, U default_value)
        : m_broker(cci::cci_get_broker()), param_cci(name.c_str(), default_value) {}

    template <typename U = T, typename std::enable_if<is_std_vector<U>::value, int>::type = 0>
    Param(const std::string& name)
        : m_broker(cci::cci_get_broker()), param_cci(name.c_str(), stored_value_type("[]")) {}

    template <typename U = T, typename std::enable_if<is_std_vector<U>::value, int>::type = 0>
    Param(const std::string& name, const U& default_value)
        : m_broker(cci::cci_get_broker()),
          param_cci(name.c_str(), vector_to_json_array(default_value)) {}

    std::string get_Name() { return param_cci.name(); }

    stored_value_type get_stored_value() { return this->param_cci; }

    T get_param_value() {
        if constexpr (is_std_vector<T>::value) {
            cci::cci_param_handle h = m_broker.get_param_handle(param_cci.name());
            if (h.is_valid()) {
                const std::string json = unquote_json_string(h.get_cci_value().to_json());
                return json_array_to_vector<T>(json);
            }
            return json_array_to_vector<T>(get_stored_value());
        } else {
            return this->param_cci;
        }
    }

    void Set_String_param(const std::string& hier_name, const std::string& value) {
        cci::cci_param_handle h = m_broker.get_param_handle(hier_name);
        if (h.is_valid()) {
            h.set_cci_value(cci::cci_value(value));
        } else {
            std::cout << "Execute: Param (" << hier_name << ") is not found!" << std::endl;
        }
    }

    std::string get_String_param(const std::string& hier_name) {
        cci::cci_param_handle h = m_broker.get_param_handle(hier_name);
        if (h.is_valid()) {
            return unquote_json_string(h.get_cci_value().to_json());
        }
        std::cout << "Execute: Param (" << hier_name << ") is not found!" << std::endl;
        return std::string();
    }

    template <typename Vec, typename std::enable_if<is_std_vector<Vec>::value, int>::type = 0>
    void Set_Vector_param(const std::string& hier_name, const Vec& value) {
        Set_String_param(hier_name, vector_to_json_array(value));
    }

    template <typename Vec, typename std::enable_if<is_std_vector<Vec>::value, int>::type = 0>
    Vec get_Vector_param(const std::string& hier_name) {
        return json_array_to_vector<Vec>(get_String_param(hier_name));
    }

    template <typename U>
    void Set_param(const std::string& hier_name, const U& value) {
        if constexpr (std::is_same_v<U, std::string>) {
            Set_String_param(hier_name, value);
        } else if constexpr (is_std_vector<U>::value) {
            Set_Vector_param<U>(hier_name, value);
        } else if constexpr (std::is_same_v<U, bool>) {
            Set_Bool_param(hier_name, value);
        } else if constexpr (std::is_same_v<U, double>) {
            Set_Double_param(hier_name, value);
        } else if constexpr (std::is_same_v<U, unsigned int>) {
            Set_UInt_param(hier_name, value);
        } else {
            Set_Int_param(hier_name, value);
        }
    }

    template <typename U>
    U get_param(const std::string& hier_name) {
        if constexpr (std::is_same_v<U, std::string>) {
            return get_String_param(hier_name);
        } else if constexpr (is_std_vector<U>::value) {
            return get_Vector_param<U>(hier_name);
        } else if constexpr (std::is_same_v<U, bool>) {
            return get_Bool_param(hier_name);
        } else if constexpr (std::is_same_v<U, double>) {
            return get_Double_param(hier_name);
        } else if constexpr (std::is_same_v<U, unsigned int>) {
            return get_UInt_param(hier_name);
        } else {
            return get_Int_param(hier_name);
        }
    }

    void Set_Int_param(std::string hier_name, T value) {
        cci::cci_param_handle h = m_broker.get_param_handle(hier_name);
        if (h.is_valid()) {
            h.set_cci_value(cci::cci_value(value));
        } else {
            std::cout << "Execute: Param (" << hier_name << ") is not found!" << std::endl;
        }
    }

    T get_Int_param(std::string hier_name) {
        T new_value{};
        cci::cci_param_handle h = m_broker.get_param_handle(hier_name);
        if (h.is_valid()) {
            new_value = stringToType(h.get_cci_value().to_json());
            std::cout << "execute: [EXTERNAL] Current value of " << h.name() << " is "
                      << new_value << std::endl;
        } else {
            std::cout << "Execute: Param (" << hier_name << ") is not found!" << std::endl;
        }
        return new_value;
    }

    void Set_UInt_param(std::string hier_name, T value) { Set_Int_param(hier_name, value); }
    T get_UInt_param(std::string hier_name) { return get_Int_param(hier_name); }
    void Set_Bool_param(std::string hier_name, T value) { Set_Int_param(hier_name, value); }
    T get_Bool_param(std::string hier_name) { return get_Int_param(hier_name); }

    void Set_Double_param(std::string hier_name, T value) {
        cci::cci_param_handle h = m_broker.get_param_handle(hier_name);
        if (h.is_valid()) {
            h.set_cci_value(cci::cci_value(value));
        } else {
            std::cout << "Execute: Param (" << hier_name << ") is not found!" << std::endl;
        }
    }

    T get_Double_param(std::string hier_name) { return get_Int_param(hier_name); }

    T stringToType(const std::string& str) {
        if constexpr (is_std_vector<T>::value) {
            using Elem = typename T::value_type;
            const auto first = str.find_first_not_of(" \f\n\r\t\v");
            if (first != std::string::npos && str[first] == '[') {
                const auto v = cci::cci_value::from_json(str);
                return v.template get<std::vector<Elem>>();
            }
            const auto v = cci::cci_value::from_json(str);
            try {
                const auto inner = v.template get<std::string>();
                const auto inner_first = inner.find_first_not_of(" \f\n\r\t\v");
                if (inner_first != std::string::npos && inner[inner_first] == '[') {
                    const auto v2 = cci::cci_value::from_json(inner);
                    return v2.template get<std::vector<Elem>>();
                }
            } catch (...) {
            }
            return T{v.template get<Elem>()};
        } else {
            T result{};
            std::stringstream ss(str);
            ss >> result;
            return result;
        }
    }
};

inline int load_config_file(const char* filename) {
    std::map<std::string, std::string> cci_parameters;
    // Static storage: the global broker outlives the test and is not a leak.
    // A heap allocation here is reported by LeakSanitizer once sc_main returns.
    static cci_utils::broker global_broker("Global Broker");
    static const bool registered = (cci::cci_register_broker(global_broker), true);
    (void)registered;
    if (filename != nullptr) {
        cci_parameters = parse_config_file(filename);
        std::ifstream f(filename);
        if (!f.is_open()) {
            std::cout << "Configuration file provided Not Found" << std::endl;
            return 0;
        }
    } else {
        std::cout << " No INI file provided, using default configuration";
    }

    cci::cci_originator m_originator("load_config_file");
    cci::cci_broker_handle m_broker(cci::cci_get_global_broker(m_originator));

    std::string instance_prefix;
    auto it = cci_parameters.find("instance_prefix");
    if (it != cci_parameters.end()) {
        instance_prefix = cci::cci_value::from_json(it->second).get<std::string>();
        if (!instance_prefix.empty() && instance_prefix.back() == '.') {
            instance_prefix.pop_back();
        }
    }

    for (auto p : cci_parameters) {
        if (p.first == "instance_prefix") continue;
        std::string json = p.second;
        const auto first = json.find_first_not_of(" \f\n\r\t\v");
        if (first != std::string::npos && json[first] == '[') {
            json = "\"" + json + "\"";
        }
        cci::cci_value val = cci::cci_value::from_json(json);
        std::cout << "Setting: " << p.first << " = " << p.second << std::endl;
        m_broker.set_preset_cci_value(p.first, val);
        if (!instance_prefix.empty() && p.first.find('.') == std::string::npos) {
            m_broker.set_preset_cci_value(instance_prefix + "." + p.first, val);
        }
    }
    return 1;
}

} // namespace regmodel
