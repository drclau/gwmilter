#pragma once

#include "config_node.hpp"
#include <memory>
#include <sstream>
#include <tuple>
#include <type_traits>

namespace cfg2 {

// Forward-declare deserialize (for nested structs)
template<typename T> T deserialize(const ConfigNode &);

// --------------------------------------------------------------------------------
// Field descriptor

template<typename Class, typename Field> struct FieldDesc {
    std::string name;
    Field Class::*ptr;
    FieldDesc(const std::string &n, Field Class::*p)
        : name(n), ptr(p)
    { }
};

template<typename Class, typename Field> FieldDesc<Class, Field> field(const std::string &name, Field Class::*ptr)
{
    return FieldDesc<Class, Field>(name, ptr);
}

// --------------------------------------------------------------------------------
// Trait to mark structs as deserializable

template<typename T> struct is_deserializable_struct : std::false_type { };

// --------------------------------------------------------------------------------
// Trait to mark structs as validatable

template<typename T> struct has_validation : std::false_type { };

// Validation function signature (default no-op implementation)
template<typename T> void validate(const T &obj)
{
    // Default implementation does nothing
}

// --------------------------------------------------------------------------------
// Deserializer implementation

template<typename T, typename... Fields> class Deserializer {
    std::tuple<Fields...> fields;

    template<std::size_t I = 0> void deserializeFields(T &obj, const ConfigNode &node) const
    {
        if constexpr (I < sizeof...(Fields)) {
            const auto &fieldDesc = std::get<I>(fields);
            const ConfigNode *childNode = node.findChild(fieldDesc.name);

            if (childNode) {
                using FieldType = std::decay_t<decltype(obj.*fieldDesc.ptr)>;

                if constexpr (is_deserializable_struct<FieldType>::value) {
                    // Nested struct
                    obj.*(fieldDesc.ptr) = deserialize<FieldType>(*childNode);
                } else if constexpr (is_vector<FieldType>::value) {
                    // std::vector<...>
                    using Elem = typename FieldType::value_type;
                    auto &vec = obj.*(fieldDesc.ptr);
                    vec.clear();

                    // Check if we have a comma-separated value or child nodes
                    if (!childNode->value.empty()) {
                        // Parse comma-separated string
                        std::stringstream ss(childNode->value);
                        std::string token;
                        while (std::getline(ss, token, ',')) {
                            // Trim whitespace
                            size_t start = token.find_first_not_of(" \t");
                            if (start == std::string::npos)
                                continue;
                            size_t end = token.find_last_not_of(" \t");
                            token = token.substr(start, end - start + 1);

                            if (!token.empty()) {
                                if constexpr (is_deserializable_struct<Elem>::value) {
                                    // Cannot deserialize structs from comma-separated strings
                                    throw std::runtime_error("Cannot parse nested structs from comma-separated values");
                                } else {
                                    vec.push_back(fromString<Elem>(token));
                                }
                            }
                        }
                    } else {
                        // Fall back to child node approach
                        for (const auto &kid: childNode->children)
                            if constexpr (is_deserializable_struct<Elem>::value)
                                vec.push_back(deserialize<Elem>(kid));
                            else
                                vec.push_back(fromString<Elem>(kid.value));
                    }
                } else {
                    // Basic type
                    obj.*(fieldDesc.ptr) = fromString<FieldType>(childNode->value);
                }
            }

            deserializeFields<I + 1>(obj, node);
        }
    }

public:
    Deserializer(Fields... f)
        : fields(f...)
    { }

    T operator()(const ConfigNode &node) const
    {
        T obj{};
        deserializeFields(obj, node);

        // Add validation if available
        if constexpr (has_validation<T>::value)
            validate(obj);

        return obj;
    }
};

// --------------------------------------------------------------------------------
// Helper to turn __VA_ARGS__ into a real template pack

template<typename T, typename... FieldTs> auto make_deserializer(FieldTs... fields) -> Deserializer<T, FieldTs...>
{
    return Deserializer<T, FieldTs...>(fields...);
}

// --------------------------------------------------------------------------------
// Registration macro

#define REGISTER_STRUCT(Type, ...)                                                                                     \
    template<> struct is_deserializable_struct<Type> : std::true_type { };                                             \
    template<> Type deserialize<Type>(const ConfigNode &node)                                                          \
    {                                                                                                                  \
        auto deserializer = make_deserializer<Type>(__VA_ARGS__);                                                      \
        return deserializer(node);                                                                                     \
    }

// --------------------------------------------------------------------------------
// Helper to kick off parsing

template<typename T> T parse(const ConfigNode &node)
{
    static_assert(is_deserializable_struct<T>::value, "Type not deserializable");
    return deserialize<T>(node);
}

} // namespace cfg2