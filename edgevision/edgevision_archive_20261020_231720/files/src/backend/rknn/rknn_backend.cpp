#include "evs/rknn_backend.h"

#if !defined(EVS_BACKEND_RKNN)

// Built without the RKNN SDK: keep the symbols linkable but report NotSupported.
// (CMake only compiles this file for EVS_BACKEND=rknn; this branch is a safety
// net so the header can be parsed anywhere.)
namespace evs {
// Empty placeholder keeps std::unique_ptr<Impl> complete for the deleter, so this
// file also compiles (and can be syntax-checked) without the RKNN SDK present.
struct RknnBackend::Impl {};

RknnBackend::RknnBackend() = default;
RknnBackend::~RknnBackend() = default;
uint32_t RknnBackend::load(const std::string&, int) {
    return static_cast<uint32_t>(EvsStatus::NotSupported);
}
uint32_t RknnBackend::infer(const std::vector<uint8_t>&, std::vector<float>&) {
    return static_cast<uint32_t>(EvsStatus::NotSupported);
}
uint32_t RknnBackend::validateDetectionModel(const std::string&, int, int, int) const {
    return static_cast<uint32_t>(EvsStatus::NotSupported);
}
uint32_t RknnBackend::inferDetectionRows(const std::vector<uint8_t>&, std::vector<float>& out,
                                         const std::string&, int, int, int) {
    out.clear();
    return static_cast<uint32_t>(EvsStatus::NotSupported);
}
void RknnBackend::release() {}
} // namespace evs

#else

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <rknn_api.h>

#include "../yolo_decode.h"
#include "detection_rows.h"
#include "evs/log.h"

namespace evs {

namespace {

// Map a YAML npu_core_mask bitfield to the RKNN core enum.
rknn_core_mask to_rknn_core(int m) {
    switch (m) {
        case 2: return RKNN_NPU_CORE_1;
        case 4: return RKNN_NPU_CORE_2;
        case 3: return RKNN_NPU_CORE_0_1;
        case 7: return RKNN_NPU_CORE_0_1_2;
        default: return RKNN_NPU_CORE_0;
    }
}

const char* type_name(rknn_tensor_type t) {
    switch (t) {
        case RKNN_TENSOR_FLOAT32: return "fp32";
        case RKNN_TENSOR_FLOAT16: return "fp16";
        case RKNN_TENSOR_INT8: return "int8";
        case RKNN_TENSOR_UINT8: return "uint8";
        case RKNN_TENSOR_INT16: return "int16";
        default: return "?";
    }
}

// Read the whole model into memory: rknn_init() takes the model bytes, not a path.
bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize n = f.tellg();
    if (n <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    return static_cast<bool>(
        f.read(reinterpret_cast<char*>(out.data()), n));
}

} // anonymous namespace

struct RknnBackend::Impl {
    rknn_context ctx = 0;

    // Input tensor description, queried from the model at load time.
    rknn_tensor_type in_type = RKNN_TENSOR_FLOAT32;
    rknn_tensor_format in_fmt = RKNN_TENSOR_NHWC;
    uint32_t in_w = 0, in_h = 0, in_c = 0;
    uint32_t in_elems = 0;

    // Number of raw output tensors (YOLOv5 exposes three heads).
    uint32_t n_output = 0;

    // Value range expected by the model input. RKNN models converted with the
    // official yolov5 recipe take raw 0..255 floats (mean/std are baked into the
    // graph); models exported from an ONNX file that already divides by 255 want
    // 0..1. Auto-detect is impossible, so it is an env switch, default 0..255.
    // NOTE: quantised models (int8/uint8 input, the common RKNN case) bypass
    // this entirely -- the uint8 buffer goes to the runtime unchanged.
    float in_scale = 1.0f;
    bool in_scale_from_env = false;

    // 0 = raw logits, 1 = sigmoid fused on objectness/class only,
    // 2 = the whole Detect post-processing is fused (see yolo_decode.h),
    // 3 = auto-detect from the value distribution (default).
    int head_mode = 3;

    std::vector<uint8_t> model_bytes;
    std::vector<float> in_f32;   // staging for float inputs
    std::vector<yolo::Head> heads;

    uint64_t last_run_us = 0;
};

RknnBackend::RknnBackend() : impl_(std::make_unique<Impl>()) {}
RknnBackend::~RknnBackend() { release(); }

uint32_t RknnBackend::load(const std::string& model_path, int /*init_timeout_s*/) {
    if (model_path.empty()) {
        EVS_LOGE("rknn:empty model path");
        return static_cast<uint32_t>(EvsStatus::InvalidParameter);
    }
    if (!read_file(model_path, impl_->model_bytes)) {
        EVS_LOGE("rknn:cannot read model %s", model_path.c_str());
        return static_cast<uint32_t>(EvsStatus::StorageRead);
    }

    int ret = rknn_init(&impl_->ctx, impl_->model_bytes.data(),
                        static_cast<uint32_t>(impl_->model_bytes.size()), 0, nullptr);
    if (ret != 0 || impl_->ctx == 0) {
        EVS_LOGE("rknn:rknn_init failed ret=%d (%s)", ret, model_path.c_str());
        impl_->ctx = 0;
        return static_cast<uint32_t>(EvsStatus::ModelLoadFailed);
    }

    // NPU core affinity: EVS_NPU_CORE_MASK (0 / unset => all cores the driver
    // picks by default). A production integration plumbs the YAML value here.
    const char* env = std::getenv("EVS_NPU_CORE_MASK");
    const int cm = env ? std::atoi(env) : 0;
    if (cm > 0) rknn_set_core_mask(impl_->ctx, to_rknn_core(cm));

    rknn_input_output_num io_num;
    std::memset(&io_num, 0, sizeof(io_num));
    ret = rknn_query(impl_->ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != 0 || io_num.n_input < 1) {
        EVS_LOGE("rknn:query IN_OUT_NUM failed ret=%d", ret);
        release();
        return static_cast<uint32_t>(EvsStatus::ModelLoadFailed);
    }
    impl_->n_output = io_num.n_output;

    rknn_tensor_attr attr;
    std::memset(&attr, 0, sizeof(attr));
    attr.index = 0;
    ret = rknn_query(impl_->ctx, RKNN_QUERY_INPUT_ATTR, &attr, sizeof(attr));
    if (ret != 0 || attr.n_dims < 3) {
        EVS_LOGE("rknn:query INPUT_ATTR failed ret=%d n_dims=%u", ret, attr.n_dims);
        release();
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    }
    if (attr.n_dims != 4 || attr.dims[0] != 1) {
        release();
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    }
    if (attr.fmt == RKNN_TENSOR_NHWC) {
        impl_->in_h = attr.dims[1]; impl_->in_w = attr.dims[2]; impl_->in_c = attr.dims[3];
    } else if (attr.fmt == RKNN_TENSOR_NCHW) {
        impl_->in_c = attr.dims[1]; impl_->in_h = attr.dims[2]; impl_->in_w = attr.dims[3];
    } else {
        release();
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    }
    impl_->in_type = attr.type;
    impl_->in_fmt = attr.fmt;
    impl_->in_elems = attr.n_elems;

    const char* sc = std::getenv("EVS_RKNN_INPUT_SCALE");
    if (sc) {
        impl_->in_scale = static_cast<float>(std::atof(sc));
        impl_->in_scale_from_env = true;
    }

    const char* hm = std::getenv("EVS_RKNN_HEAD_MODE");
    if (!hm) hm = std::getenv("EVS_YOLO_HEAD_MODE");
    impl_->head_mode = hm ? std::atoi(hm) : 3;

    EVS_LOGI("rknn:model ready: in %ux%ux%u type=%s fmt=%s elems=%u, outputs=%u, in_scale=%.4f",
             impl_->in_w, impl_->in_h, impl_->in_c, type_name(impl_->in_type),
             impl_->in_fmt == RKNN_TENSOR_NHWC ? "NHWC" : "NCHW",
             impl_->in_elems, impl_->n_output, impl_->in_scale);

    // Log the output shapes so a decode mismatch is visible in the log rather
    // than showing up as "no detections".
    for (uint32_t i = 0; i < impl_->n_output; ++i) {
        rknn_tensor_attr oa;
        std::memset(&oa, 0, sizeof(oa));
        oa.index = i;
        if (rknn_query(impl_->ctx, RKNN_QUERY_OUTPUT_ATTR, &oa, sizeof(oa)) != 0)
            continue;
        EVS_LOGI("rknn:  out[%u] name=%s dims=[%u %u %u %u] type=%s elems=%u",
                 i, oa.name, oa.n_dims > 0 ? oa.dims[0] : 0,
                 oa.n_dims > 1 ? oa.dims[1] : 0, oa.n_dims > 2 ? oa.dims[2] : 0,
                 oa.n_dims > 3 ? oa.dims[3] : 0, type_name(oa.type), oa.n_elems);
    }

    impl_->heads.resize(impl_->n_output);
    loaded_.store(true);
    return 0u;
}

uint32_t RknnBackend::infer(const std::vector<uint8_t>& input,
                            std::vector<float>& output) {
    return inferImpl(input, output, "", 80);
}

uint32_t RknnBackend::validateDetectionModel(const std::string& format, int classes,
                                              int expected_width, int expected_height) const {
    if (!loaded_.load()) return static_cast<uint32_t>(EvsStatus::NotInitialized);
    if (impl_->in_w != uint32_t(expected_width) || impl_->in_h != uint32_t(expected_height) ||
        impl_->in_c != 3 || impl_->n_output != 1 || classes < 1 || classes > 4096 ||
        (format != "flat6" && format != "yolov8" && format != "yolox"))
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    rknn_tensor_attr attr{};
    attr.index = 0;
    if (rknn_query(impl_->ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr)) != 0)
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    if (attr.n_dims > RKNN_MAX_DIMS)
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    const std::vector<size_t> shape(attr.dims, attr.dims + attr.n_dims);
    size_t rows = 0, columns = 0;
    bool transposed = false;
    if (!rknn_detection_shape(shape, format, classes, expected_width, expected_height,
                              rows, columns, transposed) || rows * columns != attr.n_elems)
        return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
    return 0;
}

uint32_t RknnBackend::inferDetectionRows(const std::vector<uint8_t>& input,
                                         std::vector<float>& output, const std::string& format,
                                         int classes, int expected_width, int expected_height) {
    output.clear();
    if (const auto rc = validateDetectionModel(format, classes, expected_width, expected_height))
        return rc;
    return inferImpl(input, output, format, classes);
}

uint32_t RknnBackend::inferImpl(const std::vector<uint8_t>& input,
                                std::vector<float>& output, const std::string& format,
                                int classes) {
    output.clear();
    if (!loaded_.load() || impl_->ctx == 0)
        return static_cast<uint32_t>(EvsStatus::NotInitialized);
    if (input.empty()) return static_cast<uint32_t>(EvsStatus::NullPointer);

    const size_t need = static_cast<size_t>(impl_->in_w) * impl_->in_h * impl_->in_c;
    if (input.size() < need) {
        EVS_LOGE("rknn:input too small: %zu < %zu", input.size(), need);
        return static_cast<uint32_t>(EvsStatus::InvalidImageSize);
    }

    rknn_input ri;
    std::memset(&ri, 0, sizeof(ri));
    ri.index = 0;
    ri.pass_through = 0; // let the driver convert layout/type to the tensor
    ri.fmt = format.empty() ? impl_->in_fmt : RKNN_TENSOR_NHWC;

    if (impl_->in_type == RKNN_TENSOR_FLOAT32) {
        // NHWC float in the model's value range; the graph carries mean/std.
        impl_->in_f32.resize(need);
        const float s = impl_->in_scale;
        for (size_t i = 0; i < need; ++i)
            impl_->in_f32[i] = static_cast<float>(input[i]) * s;
        ri.type = RKNN_TENSOR_FLOAT32;
        ri.buf = impl_->in_f32.data();
        ri.size = static_cast<uint32_t>(impl_->in_f32.size() * sizeof(float));
    } else {
        // Quantised model input: hand the uint8 buffer over untouched.
        ri.type = RKNN_TENSOR_UINT8;
        ri.buf = const_cast<uint8_t*>(input.data());
        ri.size = static_cast<uint32_t>(input.size());
    }

    int ret = rknn_inputs_set(impl_->ctx, 1, &ri);
    if (ret != 0) {
        EVS_LOGE("rknn:rknn_inputs_set ret=%d", ret);
        return static_cast<uint32_t>(EvsStatus::InferFailed);
    }

    const auto t0 = std::chrono::steady_clock::now();
    ret = rknn_run(impl_->ctx, nullptr);
    if (ret != 0) {
        EVS_LOGE("rknn:rknn_run ret=%d", ret);
        return static_cast<uint32_t>(EvsStatus::InferFailed);
    }

    std::vector<rknn_output> outs(impl_->n_output);
    for (uint32_t i = 0; i < impl_->n_output; ++i) {
        std::memset(&outs[i], 0, sizeof(rknn_output));
        outs[i].want_float = 1; // SDK dequantises INT8/uint8 outputs for us
        outs[i].index = i;
    }
    ret = rknn_outputs_get(impl_->ctx, impl_->n_output, outs.data(), nullptr);
    if (ret != 0) {
        EVS_LOGE("rknn:rknn_outputs_get ret=%d", ret);
        return static_cast<uint32_t>(EvsStatus::InferFailed);
    }

    // Always return SDK buffers, including allocation failures in decoding.
    struct OutputGuard {
        rknn_context ctx;
        std::vector<rknn_output>& values;
        ~OutputGuard() { rknn_outputs_release(ctx, uint32_t(values.size()), values.data()); }
    } guard{impl_->ctx, outs};
    if (!format.empty()) {
        rknn_tensor_attr attr{};
        attr.index = 0;
        if (impl_->n_output != 1 ||
            rknn_query(impl_->ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr)) != 0 ||
            attr.n_dims > RKNN_MAX_DIMS ||
            uint64_t(outs[0].size) < uint64_t(attr.n_elems) * sizeof(float))
            return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
        std::vector<size_t> shape;
        for (uint32_t i = 0; i < attr.n_dims; ++i) shape.push_back(attr.dims[i]);
        if (!rknn_detection_rows(static_cast<const float*>(outs[0].buf), attr.n_elems, shape,
                                  format, classes, impl_->in_w, impl_->in_h, output))
            return static_cast<uint32_t>(EvsStatus::ModelInvalidFormat);
        impl_->last_run_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - t0).count());
        return 0;
    }

    // Shapes come from the model, so ask once per output and keep a view.
    for (uint32_t i = 0; i < impl_->n_output; ++i) {
        rknn_tensor_attr oa;
        std::memset(&oa, 0, sizeof(oa));
        oa.index = i;
        yolo::Head h;
        if (rknn_query(impl_->ctx, RKNN_QUERY_OUTPUT_ATTR, &oa, sizeof(oa)) == 0) {
            h.n_dims = static_cast<int>(oa.n_dims);
            h.d0 = oa.n_dims > 0 ? static_cast<int>(oa.dims[0]) : 0;
            h.d1 = oa.n_dims > 1 ? static_cast<int>(oa.dims[1]) : 0;
            h.d2 = oa.n_dims > 2 ? static_cast<int>(oa.dims[2]) : 0;
            h.d3 = oa.n_dims > 3 ? static_cast<int>(oa.dims[3]) : 0;
            h.elems = oa.n_elems;
        }
        h.data = reinterpret_cast<const float*>(outs[i].buf);
        h.mode = static_cast<yolo::HeadMode>(impl_->head_mode);
        impl_->heads[i] = h;
    }
    // Compare against the ONNX path: dump the leading values of every head so a
    // layout/value mismatch is visible instead of showing up as "no detections".
    if (std::getenv("EVS_RKNN_DEBUG")) {
        for (size_t i = 0; i < impl_->heads.size(); ++i) {
            const yolo::Head& h = impl_->heads[i];
            const float* d = h.data;
            std::fprintf(stderr,
                         "[rknn] head[%zu] n_dims=%d dims=[%d %d %d %d] first6="
                         "%.4f %.4f %.4f %.4f %.4f %.4f\n",
                         i, h.n_dims, h.d0, h.d1, h.d2, h.d3, d[0], d[1], d[2],
                         d[3], d[4], d[5]);
        }
    }
    yolo::decode_heads(impl_->heads, output);

    impl_->last_run_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count());
    EVS_LOGD("rknn:infer done: rows=%zu run=%lluus",
             output.size() / (4 + yolo::kClasses),
             static_cast<unsigned long long>(impl_->last_run_us));
    return 0u;
}

void RknnBackend::release() {
    loaded_.store(false);
    if (impl_ && impl_->ctx != 0) {
        rknn_destroy(impl_->ctx);
        impl_->ctx = 0;
    }
    if (impl_) {
        impl_->n_output = 0;
        impl_->heads.clear();
        impl_->model_bytes.clear();
        impl_->model_bytes.shrink_to_fit();
    }
}

uint64_t RknnBackend::last_run_us() const {
    return impl_ ? impl_->last_run_us : 0;
}

bool RknnBackend::input_shape(int& w, int& h, int& c) const {
    if (!impl_ || impl_->in_w == 0) return false;
    w = static_cast<int>(impl_->in_w);
    h = static_cast<int>(impl_->in_h);
    c = static_cast<int>(impl_->in_c);
    return true;
}

} // namespace evs

#endif // EVS_BACKEND_RKNN
