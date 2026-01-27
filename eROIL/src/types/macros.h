#pragma once

namespace eroil {
    #define EROIL_NO_COPY(Type)          \
        Type(const Type&) = delete;      \
        Type& operator=(const Type&) = delete;

    #define EROIL_DEFAULT_COPY(Type)     \
        Type(const Type&) = default;     \
        Type& operator=(const Type&) = default;

    #define EROIL_DECL_COPY(Type)        \
        Type(const Type& other);         \
        Type& operator=(const Type& other);

    #define EROIL_NO_MOVE(Type)          \
        Type(Type&&) = delete;           \
        Type& operator=(Type&&) = delete;

    #define EROIL_DEFAULT_MOVE(Type)     \
        Type(Type&&) noexcept = default; \
        Type& operator=(Type&&) noexcept = default;

    #define EROIL_DECL_MOVE(Type)        \
        Type(Type&& other) noexcept;     \
        Type& operator=(Type&& other) noexcept;

    #if defined(__has_cpp_attribute)
        #if __has_cpp_attribute(nodiscard)
            #define EROIL_NODISCARD [[nodiscard]]
        #else
            #define EROIL_NODISCARD
        #endif
    #else
        #define EROIL_NODISCARD
    #endif
}