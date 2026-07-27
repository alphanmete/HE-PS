#pragma once
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <numeric>

using Profile = std::vector<double>;

inline void require_same_size(const Profile& a, const Profile& b) {
    if (a.size() != b.size()) throw std::runtime_error("Profile size mismatch");
}
inline Profile add_profiles(const Profile& a, const Profile& b) {
    require_same_size(a, b); Profile out(a.size());
    for (size_t i = 0; i < a.size(); ++i) out[i] = a[i] + b[i]; return out;
}
inline Profile sub_profiles(const Profile& a, const Profile& b) {
    require_same_size(a, b); Profile out(a.size());
    for (size_t i = 0; i < a.size(); ++i) out[i] = a[i] - b[i]; return out;
}
inline double l2_norm(const Profile& a) {
    double sum = 0.0; for (double v : a) sum += v * v; return std::sqrt(sum);
}
inline double improvement_score(const Profile& current, const Profile& candidate, const Profile& p_m) {
    return l2_norm(sub_profiles(current, p_m)) - l2_norm(sub_profiles(candidate, p_m));
}
inline Profile slice(const Profile& v, size_t begin, size_t end) {
    if (begin > end || end > v.size()) throw std::runtime_error("Invalid slice");
    return Profile(v.begin() + static_cast<long>(begin), v.begin() + static_cast<long>(end));
}
inline double sum_profile(const Profile& v) { return std::accumulate(v.begin(), v.end(), 0.0); }
inline void print_profile_prefix(const Profile& p, size_t n = 10) {
    std::cout << "[";
    for (size_t i = 0; i < std::min(n, p.size()); ++i) { if (i) std::cout << ", "; std::cout << p[i]; }
    if (p.size() > n) std::cout << ", ..."; std::cout << "]\n";
}
