#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace evs {
// One output tensor, with an explicit model contract; never infer COCO defaults.
inline bool rknn_detection_shape(const std::vector<size_t>& shape, const std::string& format,
                                 int classes, int width, int height, size_t& rows, size_t& columns,
                                 bool& transposed) {
    rows = columns = 0;
    transposed = false;
    if (classes < 1 || classes > 4096 || width <= 0 || height <= 0)
        return false;
    if (format != "flat6" && format != "yolov8" && format != "yolox")
        return false;
    size_t a = 0, b = 0;
    if (shape.size() == 3 && shape[0] == 1) {
        a = shape[1];
        b = shape[2];
    } else if (shape.size() == 2) {
        a = shape[0];
        b = shape[1];
    } else
        return false;
    columns = format == "flat6" ? 6 : size_t((format == "yolox" ? 5 : 4) + classes);
    transposed = format == "yolov8" && a == columns && b != columns;
    rows = transposed ? b : a;
    if ((!transposed && b != columns) || !rows || rows > 100000)
        return false;
    if (format == "yolox") {
        if (width % 32 || height % 32)
            return false;
        uint64_t expected = 0;
        for (int stride : {8, 16, 32})
            expected += uint64_t(width / stride) * uint64_t(height / stride);
        if (rows != expected)
            return false;
    }
    return true;
}

inline bool rknn_detection_rows(const float* data, size_t elements,
                                const std::vector<size_t>& shape, const std::string& format,
                                int classes, int width, int height, std::vector<float>& out) {
    out.clear();
    size_t rows = 0, columns = 0;
    bool transposed = false;
    if (!data ||
        !rknn_detection_shape(shape, format, classes, width, height, rows, columns, transposed) ||
        elements != rows * columns)
        return false;
    if (format == "yolox") {
        // Packed raw YOLOX: tx,ty,tw,th,objectness,class probabilities.
        // Objectness and classes already passed sigmoid in this model's graph.
        for (size_t i = 0; i < elements; ++i) {
            if (!std::isfinite(data[i]) || (i % columns >= 4 && (data[i] < 0 || data[i] > 1)))
                return false;
        }
        const size_t decoded_columns = size_t(4 + classes);
        out.assign(rows * decoded_columns, 0);
        size_t row = 0;
        for (int stride : {8, 16, 32}) {
            for (int y = 0; y < height / stride; ++y) {
                for (int x = 0; x < width / stride; ++x, ++row) {
                    const float* raw = data + row * columns;
                    float* decoded = out.data() + row * decoded_columns;
                    float max_score = 0;
                    for (int c = 0; c < classes; ++c) {
                        decoded[4 + c] = raw[4] * raw[5 + c];
                        max_score = std::max(max_score, decoded[4 + c]);
                    }
                    if (max_score == 0)
                        continue;
                    const double box[] = {(double(raw[0]) + x) * stride / width,
                                          (double(raw[1]) + y) * stride / height,
                                          std::exp(double(raw[2])) * stride / width,
                                          std::exp(double(raw[3])) * stride / height};
                    for (size_t j = 0; j < 4; ++j) {
                        if (!std::isfinite(box[j]) ||
                            std::abs(box[j]) > std::numeric_limits<float>::max() ||
                            (j >= 2 && box[j] < std::numeric_limits<float>::min())) {
                            out.clear();
                            return false;
                        }
                        decoded[j] = float(box[j]);
                    }
                }
            }
        }
        return true;
    }
    out.resize(elements);
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < columns; ++j) {
            float value = data[transposed ? j * rows + i : i * columns + j];
            if (!std::isfinite(value)) {
                out.clear();
                return false;
            }
            if (format == "yolov8") {
                if (j < 4)
                    value /= float(j % 2 == 0 ? width : height);
                else if (value < 0 || value > 1) {
                    out.clear();
                    return false;
                }
            } else {
                if (j == 0 && (value < 0 || value >= classes || std::floor(value) != value)) {
                    out.clear();
                    return false;
                }
                if (j == 1 && (value < 0 || value > 1)) {
                    out.clear();
                    return false;
                }
            }
            out[i * columns + j] = value;
        }
    }
    return true;
}
} // namespace evs
