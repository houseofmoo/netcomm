#pragma once

namespace eroil {
    // copy/move constructor
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

    // force return value to be used
    #if defined(__has_cpp_attribute)
        #if __has_cpp_attribute(nodiscard)
            #define NO_DISCARD [[nodiscard]]
        #else
            #define NO_DISCARD
        #endif
    #else
        #define NO_DISCARD
    #endif

    // warning conversion surpression (DONT ABUSE!)
    #if defined(__clang__)
        #define DIAG_PUSH _Pragma("clang diagnostic push")
        #define DIAG_POP  _Pragma("clang diagnostic pop")
        #define DIAG_IGN_CONV _Pragma("clang diagnostic ignored \"-Wconversion\"") \
                              _Pragma("clang diagnostic ignored \"-Wsign-conversion\"")
    #elif defined(__GNUC__)
        #define DIAG_PUSH _Pragma("GCC diagnostic push")
        #define DIAG_POP  _Pragma("GCC diagnostic pop")
        #define DIAG_IGN_CONV _Pragma("GCC diagnostic ignored \"-Wconversion\"") \
                              _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
    #else
        #define DIAG_PUSH
        #define DIAG_POP
        #define DIAG_IGN_CONV
    #endif
}