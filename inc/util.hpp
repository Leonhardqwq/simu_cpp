#pragma once
#include <cstddef>
#include <vector>
#include <cstring>
#include <string>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <limits>

const int FIXED_FLOAT_N = 10;
const int NONFIXED_FLOAT_N = 10;

template <typename To, typename From>
To bit_cast(const From& src) {
    static_assert(sizeof(To) == sizeof(From), "Size mismatch in bitcast");
    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

template<typename T>
void write_vector_to_csv(const std::vector<T>& data, const std::string& filename, bool fixed=false) {
    std::ofstream fout(filename);
    if (!fout) return;
    fout << "index,value\n";
    if (fixed)  fout << std::fixed << std::setprecision(FIXED_FLOAT_N);
    else        fout << std::scientific << std::setprecision(NONFIXED_FLOAT_N);
    for (size_t i = 0; i < data.size(); ++i) 
        fout << i << "," << data[i] << "\n";
    fout.close();
}

template<typename T>
void write_2dvector_to_csv(const std::vector<std::vector<T>>& data, const std::string& filename, bool fixed=false) {
    std::ofstream fout(filename);
    if (!fout) return;

    fout << "index";
    for(size_t i =0; i < data.size(); ++i) 
        fout << ",value" << i;
    fout << "\n";

    if (fixed)  fout << std::fixed << std::setprecision(FIXED_FLOAT_N);
    else        fout << std::scientific << std::setprecision(NONFIXED_FLOAT_N);
    for (size_t i = 0; i < data[0].size(); ++i) {
        fout << i;
        for (size_t j = 0; j < data.size(); ++j) 
            fout << "," << data[j][i];
        fout << "\n";
    }
    fout.close();
}

template<typename T>
T parse_csv_cell(const std::string& raw) {
    std::string s = raw;
    if (!s.empty() && s.back() == '\r') s.pop_back(); // 兼容 Windows 行尾

    if (s.empty()) {
        if (std::is_floating_point<T>::value) {
            return std::numeric_limits<T>::quiet_NaN();
        }
        return T{};
    }

    std::stringstream ss(s);
    T v{};
    ss >> v;
    if (ss.fail()) {
        throw std::runtime_error("CSV parse failed: " + s);
    }
    return v;
}

template<typename T>
std::vector<T> read_vector_from_csv(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) return {};

    std::string line;
    std::getline(fin, line); // 跳过表头: index,value

    std::vector<T> data;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idx, val;
        std::getline(ss, idx, ','); // 忽略 index
        std::getline(ss, val);      // value
        data.push_back(parse_csv_cell<T>(val));
    }
    return data;
}

// 对应 write_2dvector_to_csv：返回 data[col][row]
template<typename T>
std::vector<std::vector<T>> read_2dvector_from_csv(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) return {};

    std::string header;
    if (!std::getline(fin, header)) return {};

    // 统计列数（去掉 index 列）
    size_t commas = 0;
    for (char ch : header) if (ch == ',') ++commas;
    size_t cols = commas; // index + cols 个字段 => commas == cols
    std::vector<std::vector<T>> data(cols);

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);

        std::string cell;
        std::getline(ss, cell, ','); // 忽略 index

        for (size_t c = 0; c < cols; ++c) {
            if (c + 1 < cols) std::getline(ss, cell, ',');
            else std::getline(ss, cell); // 最后一列读到行尾
            data[c].push_back(parse_csv_cell<T>(cell));
        }
    }
    return data;
}

template<typename T>
void write_2dvector_to_bin(const std::vector<std::vector<T>>& data, const std::string& filename) {
    if (data.empty() || data[0].empty()) return;
    size_t cols = data.size();
    size_t rows = data[0].size();

    std::ofstream fout(filename, std::ios::binary);
    if (!fout) return;

    fout.write(reinterpret_cast<const char*>(&cols), sizeof(cols));
    fout.write(reinterpret_cast<const char*>(&rows), sizeof(rows));
    for (size_t c = 0; c < cols; ++c) {
        fout.write(
            reinterpret_cast<const char*>(data[c].data()),
            rows * sizeof(T)
        );
    }
}

std::pair<double, double> wilson_confidence_interval(double p, int total, double z=1.959963984540054) {
    double a = total + z * z;
    double b = -2 * total * p - z * z;
    double c = total * p * p;
    double lower_bound = (-b - std::sqrt(b * b - 4 * a * c)) / (2 * a);
    double upper_bound = (-b + std::sqrt(b * b - 4 * a * c)) / (2 * a);
    return {lower_bound, upper_bound};
}
