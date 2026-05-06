#pragma once

#ifdef SNPS_SCML_LIB
#undef ACCELLERA_CCI_STD
#include <scml2.h>
#include <scmlinc/scml_property.h>
#include <scmlinc/scml_property_registry.h>
#include <scmlinc/scml_simple_property_server.h>
#endif

#ifdef ACCELLERA_CCI_STD
#include <cci_configuration>
#include "config_parser.h"
#endif

#include <iostream>
#include <fstream>
#include<sstream>
#include <systemc.h>
#include <string>
#include <vector>
#include <type_traits>
#include <cstdint>
#include <cctype>

#ifdef SNPS_SCML_LIB
// Synopsys scml_property.h fully specializes unsigned long long but not uint64_t
// (on LP64, uint64_t is unsigned long). Map to a supported SCML property type.
template <typename T>
struct csml_snps_scml_property_elem {
  using type = T;
};
template <>
struct csml_snps_scml_property_elem<uint64_t> {
  using type = unsigned long long;
};
#endif

template <typename U>
struct csml_is_std_vector : std::false_type {};

template <typename Elem, typename Alloc>
struct csml_is_std_vector<std::vector<Elem, Alloc>> : std::true_type {};

template<typename T>
class csml_param
{
  private:
  #ifdef ACCELLERA_CCI_STD
      cci::cci_broker_handle m_broker; ///< CCI configuration handle
  #endif

	using stored_value_type = typename std::conditional<csml_is_std_vector<T>::value, std::string, T>::type;

	#ifdef SNPS_SCML_LIB
	// SCML only provides full specializations for select scalar types and std::string; vectors are stored as JSON strings.
	using snps_scml_property_value_type = typename std::conditional<
		csml_is_std_vector<T>::value,
		std::string,
		typename csml_snps_scml_property_elem<T>::type>::type;
	#endif

	#ifdef SNPS_SCML_LIB
		template <typename Dummy = void>
		static auto scml_set_string_property(
			scml_property_registry::PropertyType type,
			const std::string& scHierName,
			const std::string& name,
			const std::string& value,
			int) -> decltype(scml_property_registry::inst().setStringProperty(type, scHierName.c_str(), name.c_str(), value), void()) {
			scml_property_registry::inst().setStringProperty(type, scHierName.c_str(), name.c_str(), value);
		}

		template <typename Dummy = void>
		static void scml_set_string_property(
			scml_property_registry::PropertyType,
			const std::string&,
			const std::string&,
			const std::string&,
			...) {
			std::cout << "Execute: SCML setStringProperty not available in this SCML version" << std::endl;
		}
	#endif

  #ifdef SNPS_SCML_LIB
    scml_property<snps_scml_property_value_type> param_scml; ///< SCML parameter
  #else
    cci::cci_param<stored_value_type> param_cci; ///< CCI parameter
  #endif

  public:
	template <typename U>
	using is_std_vector = csml_is_std_vector<U>;

	static std::string trim_copy(const std::string& s) {
		std::size_t b = 0;
		while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
			++b;
		}
		std::size_t e = s.size();
		while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
			--e;
		}
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
		if (!cur.empty() || (!out.empty())) {
			out.push_back(trim_copy(cur));
		}
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
				if (c == '\\' || c == '"') {
					out.push_back('\\');
				}
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
			if (i != 0) {
				out += ",";
			}
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
			// Backward-compatible behavior: allow scalar values for vector params
			// Example: targets = "../../../path/to/file" should become a 1-element vector.
			if (!t.empty()) {
				out.push_back(json_token_to_elem<typename Vec::value_type>(t));
			}
			return out;
		}
		t = trim_copy(t.substr(1, t.size() - 2));
		if (t.empty()) {
			return out;
		}
		const auto tokens = split_top_level_csv(t);
		out.reserve(tokens.size());
		for (const auto& tok : tokens) {
			out.push_back(json_token_to_elem<typename Vec::value_type>(tok));
		}
		return out;
	}

	//for specific module
  #ifdef SNPS_SCML_LIB
		template <typename U = T, typename std::enable_if<!csml_is_std_vector<U>::value, int>::type = 0>
		csml_param(const std::string& name):param_scml(name.c_str()){
		}
		template <typename U = T, typename std::enable_if<!csml_is_std_vector<U>::value, int>::type = 0>
		csml_param(const std::string& name, U default_value):param_scml(name.c_str(),default_value){
		}

		template <typename U = T, typename std::enable_if<csml_is_std_vector<U>::value, int>::type = 0>
		csml_param(const std::string& name):param_scml(name.c_str(), std::string("[]")){
		}
		template <typename U = T, typename std::enable_if<csml_is_std_vector<U>::value, int>::type = 0>
		csml_param(const std::string& name, const U& default_value)
			: param_scml(name.c_str(), vector_to_json_array(default_value)){
		}
  
  #else
		template <typename U = T, typename std::enable_if<!csml_is_std_vector<U>::value && !std::is_same<stored_value_type, std::string>::value, int>::type = 0>
		csml_param(const std::string& name):m_broker(cci::cci_get_broker()),
		param_cci(name.c_str()){
		}
		template <typename U = T, typename std::enable_if<!csml_is_std_vector<U>::value && !std::is_same<stored_value_type, std::string>::value, int>::type = 0>
		csml_param(const std::string& name, U default_value):m_broker(cci::cci_get_broker()),
		param_cci(name.c_str(),default_value){
		}

		template <typename U = T, typename std::enable_if<!csml_is_std_vector<U>::value && std::is_same<stored_value_type, std::string>::value, int>::type = 0>
		csml_param(const std::string& name):m_broker(cci::cci_get_broker()),
		param_cci(name.c_str(), stored_value_type()){
		}
		template <typename U = T, typename std::enable_if<!csml_is_std_vector<U>::value && std::is_same<stored_value_type, std::string>::value, int>::type = 0>
		csml_param(const std::string& name, U default_value):m_broker(cci::cci_get_broker()),
		param_cci(name.c_str(), default_value){
		}

		template <typename U = T, typename std::enable_if<csml_is_std_vector<U>::value, int>::type = 0>
		csml_param(const std::string& name):m_broker(cci::cci_get_broker()),
		param_cci(name.c_str(), stored_value_type("[]")){
		}
		template <typename U = T, typename std::enable_if<csml_is_std_vector<U>::value, int>::type = 0>
		csml_param(const std::string& name, const U& default_value):m_broker(cci::cci_get_broker()),
		param_cci(name.c_str(), vector_to_json_array(default_value)){
		}
  #endif
  
  std::string get_Name(){
    #ifdef SNPS_SCML_LIB
      return param_scml.getName();
    #else
      return param_cci.name();
    #endif
  }

	stored_value_type get_stored_value() {
		#ifdef SNPS_SCML_LIB
			if constexpr (std::is_same_v<T, uint64_t>) {
				return static_cast<T>(static_cast<unsigned long long>(this->param_scml));
			}
			return static_cast<stored_value_type>(this->param_scml);
		#else
			return this->param_cci;
		#endif
	}

	T get_param_value(){
		if constexpr (csml_is_std_vector<T>::value) {
			#ifdef SNPS_SCML_LIB
				const stored_value_type json = get_stored_value();
				return json_array_to_vector<T>(json);
			#else
				// Under CCI, the preset value may be a scalar (e.g. 8888) even if the
				// parameter storage type is string (JSON) for vectors.
				// Fetch the raw cci_value from the broker and parse its JSON.
				cci::cci_param_handle h = m_broker.get_param_handle(param_cci.name());
				if (h.is_valid()) {
					const std::string json = unquote_json_string(h.get_cci_value().to_json());
					return json_array_to_vector<T>(json);
				}
				const stored_value_type json = get_stored_value();
				return json_array_to_vector<T>(json);
			#endif
		} else {
			#ifdef SNPS_SCML_LIB
				if constexpr (std::is_same_v<T, uint64_t>) {
					return static_cast<T>(static_cast<unsigned long long>(this->param_scml));
				}
				return static_cast<T>(this->param_scml);
			#else
				return this->param_cci;
			#endif
		}
	}

	void Set_String_param(const std::string& hier_name, const std::string& value) {
		#ifdef SNPS_SCML_LIB
			const std::string s = hier_name.c_str();
			const int pos = s.find(".");
			const std::string module_name = s.substr(0, pos);
			const std::string param_name = s.substr(pos + 1);
			scml_set_string_property(scml_property_registry::MODULE, module_name, param_name, value, 0);
		#else
			const std::string param_name = hier_name.c_str();
			cci::cci_param_handle h = m_broker.get_param_handle(param_name);
			if (h.is_valid()) {
				h.set_cci_value(cci::cci_value(value));
			} else {
				std::cout << "Execute: Param (" << param_name << ") is not found!" << std::endl;
			}
		#endif
	}

	std::string get_String_param(const std::string& hier_name) {
		#ifdef SNPS_SCML_LIB
			const std::string s = hier_name.c_str();
			const int pos = s.find(".");
			const std::string module_name = s.substr(0, pos);
			const std::string param_name = s.substr(pos + 1);
			return scml_property_registry::inst().getStringProperty(
				scml_property_registry::MODULE, module_name.c_str(), param_name.c_str());
		#else
			const std::string param_name = hier_name.c_str();
			cci::cci_param_handle h = m_broker.get_param_handle(param_name);
			if (h.is_valid()) {
				return unquote_json_string(h.get_cci_value().to_json());
			}
			std::cout << "Execute: Param (" << param_name << ") is not found!" << std::endl;
			return std::string();
		#endif
	}

	template <typename Vec, typename std::enable_if<is_std_vector<Vec>::value, int>::type = 0>
	void Set_Vector_param(const std::string& hier_name, const Vec& value) {
		const std::string json = vector_to_json_array(value);
		Set_String_param(hier_name, json);
	}

	template <typename Vec, typename std::enable_if<is_std_vector<Vec>::value, int>::type = 0>
	Vec get_Vector_param(const std::string& hier_name) {
		const std::string json = get_String_param(hier_name);
		return json_array_to_vector<Vec>(json);
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

void Set_Int_param(std::string hier_name,T value){
  #ifdef SNPS_SCML_LIB
    T update_val = value;
    // hier_name holds complete hierarchical name
    std::string s = hier_name.c_str();
    // Seperate module-name and param-name
    int pos = s.find(".");
    std::string module_name = s.substr(0,pos);
    std::string param_name = s.substr(pos + 1);

      // Update the param's value to 20
     //set value of "hier_name"
    scml_property_registry::inst().setIntProperty(
     scml_property_registry::MODULE, module_name.c_str(), param_name.c_str(),update_val);
  #else
    std::cout<<"Inside Set_Int_param "<<std::endl;
    const std::string int_param_name = hier_name.c_str();
    // Wait for a while to update param value
    // Check for existence of the param
    cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
    if (int_param_handle.is_valid()) {
      // Update the param's value to 2
      int_param_handle.set_cci_value(cci::cci_value(value));
    } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
     // XREPORT_ERROR("execute: Param (" << int_param_name<< ") is not found!");
    }
  
  #endif
  }

  T get_Int_param(std::string hier_name){
    #ifdef SNPS_SCML_LIB
      // std::cout<<"Inside get_Int_param "<<std::endl;
      // hier_name holds complete hierarchical name
      std::string s = hier_name.c_str();
      // Seperate module-name and param-name
      int pos = s.find(".");
      std::string module_name = s.substr(0,pos);
      std::string param_name = s.substr(pos + 1);

      //get value of "hier_name"
      T value = scml_property_registry::inst().getIntProperty(
                        scml_property_registry::MODULE, module_name.c_str(), param_name.c_str());
      return value;

    #else
      // std::cout<<"Inside get_Int_param "<<std::endl;
      T new_value ;
      std::string int_param_name = hier_name.c_str();
      //  Check for existence of the param
      cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
      if  (int_param_handle.is_valid()) {
        new_value = stringToType(int_param_handle.get_cci_value().to_json());
        // Display new value
        std::cout << "execute: [EXTERNAL] Current value of "
                << int_param_handle.name() << " is " << new_value << std::endl;
        // stringToType(new_value);
      } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
    }
    return new_value;
    #endif
  }
  
void Set_UInt_param(std::string hier_name,T value){
  #ifdef SNPS_SCML_LIB
    T update_val = value;
    // hier_name holds complete hierarchical name
    std::string s = hier_name.c_str();
    // Seperate module-name and param-name
    int pos = s.find(".");
    std::string module_name = s.substr(0,pos);
    std::string param_name = s.substr(pos + 1);

      // Update the param's value to 20
     //set value of "hier_name"
    scml_property_registry::inst().setUIntProperty(
     scml_property_registry::MODULE, module_name.c_str(), param_name.c_str(),update_val);
  #else
    std::cout<<"Inside Set_Int_param "<<std::endl;
    const std::string int_param_name = hier_name.c_str();
    // Wait for a while to update param value
    // Check for existence of the param
    cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
    if (int_param_handle.is_valid()) {
      // Update the param's value to 2
      int_param_handle.set_cci_value(cci::cci_value(value));
    } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
     // XREPORT_ERROR("execute: Param (" << int_param_name<< ") is not found!");
    }
  
  #endif
  }

  T get_UInt_param(std::string hier_name){
    #ifdef SNPS_SCML_LIB
      // std::cout<<"Inside get_Int_param "<<std::endl;
      // hier_name holds complete hierarchical name
      std::string s = hier_name.c_str();
      // Seperate module-name and param-name
      int pos = s.find(".");
      std::string module_name = s.substr(0,pos);
      std::string param_name = s.substr(pos + 1);

      //get value of "hier_name"
      T value = scml_property_registry::inst().getUIntProperty(
                        scml_property_registry::MODULE, module_name.c_str(), param_name.c_str());
      return value;

    #else
      // std::cout<<"Inside get_Int_param "<<std::endl;
      T new_value ;
      std::string int_param_name = hier_name.c_str();
      //  Check for existence of the param
      cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
      if  (int_param_handle.is_valid()) {
        new_value = stringToType(int_param_handle.get_cci_value().to_json());
        // Display new value
        std::cout << "execute: [EXTERNAL] Current value of "
                << int_param_handle.name() << " is " << new_value << std::endl;
        // stringToType(new_value);
      } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
    }
    return new_value;
    #endif
  }
 
void Set_Bool_param(std::string hier_name,T value){
  #ifdef SNPS_SCML_LIB
    T update_val = value;
    // hier_name holds complete hierarchical name
    std::string s = hier_name.c_str();
    // Seperate module-name and param-name
    int pos = s.find(".");
    std::string module_name = s.substr(0,pos);
    std::string param_name = s.substr(pos + 1);

      // Update the param's value to 20
     //set value of "hier_name"
    scml_property_registry::inst().setBoolProperty(
     scml_property_registry::MODULE, module_name.c_str(), param_name.c_str(),update_val);
  #else
    std::cout<<"Inside Set_Int_param "<<std::endl;
    const std::string int_param_name = hier_name.c_str();
    // Wait for a while to update param value
    // Check for existence of the param
    cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
    if (int_param_handle.is_valid()) {
      // Update the param's value to 2
      int_param_handle.set_cci_value(cci::cci_value(value));
    } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
     // XREPORT_ERROR("execute: Param (" << int_param_name<< ") is not found!");
    }
  
  #endif
  }

  T get_Bool_param(std::string hier_name){
    #ifdef SNPS_SCML_LIB
      // std::cout<<"Inside get_Int_param "<<std::endl;
      // hier_name holds complete hierarchical name
      std::string s = hier_name.c_str();
      // Seperate module-name and param-name
      int pos = s.find(".");
      std::string module_name = s.substr(0,pos);
      std::string param_name = s.substr(pos + 1);

      //get value of "hier_name"
      T value = scml_property_registry::inst().getBoolProperty(
                        scml_property_registry::MODULE, module_name.c_str(), param_name.c_str());
      return value;

    #else
      // std::cout<<"Inside get_Int_param "<<std::endl;
      T new_value ;
      std::string int_param_name = hier_name.c_str();
      //  Check for existence of the param
      cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
      if  (int_param_handle.is_valid()) {
        new_value = stringToType(int_param_handle.get_cci_value().to_json());
        // Display new value
        std::cout << "execute: [EXTERNAL] Current value of "
                << int_param_handle.name() << " is " << new_value << std::endl;
        // stringToType(new_value);
      } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
    }
    return new_value;
    #endif
  }

void Set_Double_param(std::string hier_name,T value){
  #ifdef SNPS_SCML_LIB
    T update_val = value;
    // hier_name holds complete hierarchical name
    std::string s = hier_name.c_str();
    // Seperate module-name and param-name
    int pos = s.find(".");
    std::string module_name = s.substr(0,pos);
    std::string param_name = s.substr(pos + 1);

      // Update the param's value to 20
     //set value of "hier_name"
    scml_property_registry::inst().setDoubleProperty(
     scml_property_registry::MODULE, module_name.c_str(), param_name.c_str(),update_val);
  #else
    std::cout<<"Inside Set_Double_param "<<std::endl;
    const std::string double_param_name = hier_name.c_str();
    // Wait for a while to update param value
    // Check for existence of the param
    cci::cci_param_handle double_param_handle = m_broker.get_param_handle(double_param_name);
    if (double_param_handle.is_valid()) {
      // Update the param's value to 2
      double_param_handle.set_cci_value(cci::cci_value(value));
    } else {
      std::cout << "Execute: Param (" << double_param_name<< ") is not found!"<< std::endl;
     // XREPORT_ERROR("execute: Param (" << int_param_name<< ") is not found!");
    }
  
  #endif
  }

  T get_Double_param(std::string hier_name){
    #ifdef SNPS_SCML_LIB
      // std::cout<<"Inside get_Int_param "<<std::endl;
      // hier_name holds complete hierarchical name
      std::string s = hier_name.c_str();
      // Seperate module-name and param-name
      int pos = s.find(".");
      std::string module_name = s.substr(0,pos);
      std::string param_name = s.substr(pos + 1);

      //get value of "hier_name"
      T value = scml_property_registry::inst().getDoubleProperty(
                        scml_property_registry::MODULE, module_name.c_str(), param_name.c_str());
      return value;

    #else
      // std::cout<<"Inside get_Int_param "<<std::endl;
      T new_value ;
      std::string int_param_name = hier_name.c_str();
      //  Check for existence of the param
      cci::cci_param_handle int_param_handle = m_broker.get_param_handle(int_param_name);
      if  (int_param_handle.is_valid()) {
        new_value = stringToType(int_param_handle.get_cci_value().to_json());
        // Display new value
        std::cout << "execute: [EXTERNAL] Current value of "
                << int_param_handle.name() << " is " << new_value << std::endl;
        // stringToType(new_value);
      } else {
      std::cout << "Execute: Param (" << int_param_name<< ") is not found!"<< std::endl;
    }
    return new_value;
    #endif
  }

  // convert default string type of cci::get_cci_value to 'T' type
  T stringToType(const std::string& str) {
    if constexpr (csml_is_std_vector<T>::value) {
      using Elem = typename T::value_type;
      const auto first = str.find_first_not_of(" \f\n\r\t\v");
      if(first != std::string::npos && str[first] == '[') {
        const auto v = cci::cci_value::from_json(str);
        return v.get<std::vector<Elem>>();
      }

      // Some vector params are stored as string presets (because CCI 1.0.1 cannot always
      // apply JSON arrays directly as preset values to parameters). Handle: "[4000]".
      const auto v = cci::cci_value::from_json(str);
      try {
        const auto inner = v.get<std::string>();
        const auto inner_first = inner.find_first_not_of(" \f\n\r\t\v");
        if(inner_first != std::string::npos && inner[inner_first] == '[') {
          const auto v2 = cci::cci_value::from_json(inner);
          return v2.get<std::vector<Elem>>();
        }
      } catch(...) {
      }

      return T{v.get<Elem>()};
    } else {
      T result;
      std::stringstream ss(str);
      ss >> result;
      return result;
    }
  }

  ~csml_param(){};

};

inline  int load_config_file(const char *filename) {
#ifdef ACCELLERA_CCI_STD
    // Handle parsing of the input ini file for cci parameter values
    // Using the values from the ini, set the parameters to the new value
    std::map<std::string,std::string> cci_parameters;
    cci::cci_register_broker(new cci_utils::broker("Global Broker"));
    if(filename != NULL)
    {
        cci_parameters = cci_configfile_parser(filename);
        std::ifstream f(filename);
        if(!f.is_open())
        {
            std::cout << "Configuration file provided Not Found" << std::endl;
            return 0;
        }
    }
    else
    {
        std::cout<<" No INI file provided, using default configuration";
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

    for(auto p: cci_parameters)
    {
        if (p.first == "instance_prefix") {
            continue;
        }
        std::string json = p.second;
        const auto first = json.find_first_not_of(" \f\n\r\t\v");
        if(first != std::string::npos && json[first] == '[') {
            // Preserve JSON array payloads as strings for preset application.
            json = "\"" + json + "\"";
        }
        cci::cci_value val = cci::cci_value::from_json(json);
        std::cout << "Setting: " << p.first << " = " << p.second << std::endl;
        m_broker.set_preset_cci_value(p.first,val);
        if (!instance_prefix.empty() && p.first.find('.') == std::string::npos) {
            m_broker.set_preset_cci_value(instance_prefix + "." + p.first, val);
        }
    }
#elif defined(SNPS_SCML_LIB)
    // Synopsys scml_simple_property_server::load() returns false for empty files and often for
    // comment/whitespace-only files — that is not an invalid "format", just nothing to parse.
    static scml_simple_property_server* s_ps = nullptr;
    if (s_ps == nullptr) {
        s_ps = new scml_simple_property_server();
        scml_property_registry::inst().setCustomPropertyServer(s_ps);
    }
    if (filename != nullptr) {
        if(s_ps->load(filename))  {
                std::cout << "SCML parameter configuration loaded: " << filename << std::endl;
                } else {
                std::cout << "SCML parameter configuration file: " << filename << std::endl;
                SC_REPORT_FATAL("och_sep_ss", "Error loading SCML parameter configuration file");
                }
                } else {
        SC_REPORT_FATAL("sc_main", "SCML parameter configuration file not provided");
    }
#endif
    return 1;
}



