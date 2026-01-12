// #pragma once
// #include <vector>
// #include <utility>
// #include <algorithm>

// namespace eroil {
//     template <typename T>
//     std::pair<T*, bool> find_ptr(std::vector<T>& v, const T& value) {
//         auto it = std::find(v.begin(), v.end(), value);
//         if (it == v.end()) return {nullptr, false};
//         return {&(*it), true};
//     }
// }