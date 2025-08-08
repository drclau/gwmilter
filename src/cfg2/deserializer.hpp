#pragma once

#include "tree_node.hpp"
#include <tuple>
#include <type_traits>
#include <memory>

namespace cfg2 {

// Forward-declare deserialize (for nested structs)
template<typename T>
T deserialize(const TreeNode&);

// --------------------------------------------------------------------------------
// Field descriptor

template<typename Class, typename Field>
struct FieldDesc {
    std::string name;
    Field Class::* ptr;
    FieldDesc(const std::string& n, Field Class::* p)
      : name(n), ptr(p) {}
};

template<typename Class, typename Field>
FieldDesc<Class, Field> field(const std::string& name, Field Class::* ptr) {
    return FieldDesc<Class, Field>(name, ptr);
}

// --------------------------------------------------------------------------------
// Trait to mark structs as deserializable

template<typename T>
struct is_deserializable_struct : std::false_type {};

// --------------------------------------------------------------------------------
// Deserializer implementation

template<typename T, typename... Fields>
class Deserializer {
    std::tuple<Fields...> fields;
    
    template<std::size_t I = 0>
    void deserializeFields(T& obj, const TreeNode& node) const {
        if constexpr (I < sizeof...(Fields)) {
            const auto& fieldDesc = std::get<I>(fields);
            const TreeNode* childNode = node.findChild(fieldDesc.name);
            
            if (childNode) {
                using FieldType = std::decay_t<decltype(obj.*fieldDesc.ptr)>;

                if constexpr (is_deserializable_struct<FieldType>::value) {
                    // Nested struct
                    obj.*(fieldDesc.ptr) = deserialize<FieldType>(*childNode);
                }
                else if constexpr (is_vector<FieldType>::value) {
                    // std::vector<...>
                    using Elem = typename FieldType::value_type;
                    auto& vec = obj.*(fieldDesc.ptr);
                    vec.clear();
                    
                    for (const auto& kid : childNode->children) {
                        if constexpr (is_deserializable_struct<Elem>::value) {
                            vec.push_back(deserialize<Elem>(kid));
                        } else {
                            vec.push_back(fromString<Elem>(kid.value));
                        }
                    }
                }
                else {
                    // Basic type
                    obj.*(fieldDesc.ptr) = fromString<FieldType>(childNode->value);
                }
            }
            
            deserializeFields<I + 1>(obj, node);
        }
    }
    
public:
    Deserializer(Fields... f) : fields(f...) {}
    
    T operator()(const TreeNode& node) const {
        T obj{};
        deserializeFields(obj, node);
        return obj;
    }
};

// --------------------------------------------------------------------------------
// Helper to turn __VA_ARGS__ into a real template pack

template<typename T, typename... FieldTs>
auto make_deserializer(FieldTs... fields)
  -> Deserializer<T, FieldTs...>
{
    return Deserializer<T, FieldTs...>(fields...);
}

// --------------------------------------------------------------------------------
// Registration macro

#define REGISTER_STRUCT(Type, ...)                                \
    template<>                                                    \
    struct is_deserializable_struct<Type> : std::true_type {};    \
    template<>                                                    \
    Type deserialize<Type>(const TreeNode& node) {                \
        auto deserializer = make_deserializer<Type>(__VA_ARGS__); \
        return deserializer(node);                                \
    }

// --------------------------------------------------------------------------------
// Helper to kick off parsing

template<typename T>
T parse(const TreeNode& node) {
    static_assert(is_deserializable_struct<T>::value, "Type not deserializable");
    return deserialize<T>(node);
}

} // namespace cfg2