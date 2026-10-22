#include "evs/backend_factory.h"

#if defined(EVS_BACKEND_MOCK)
#include "evs/mock_backend.h"
#endif
#if defined(EVS_BACKEND_RKNN)
#include "evs/rknn_backend.h"
#endif
#if defined(EVS_BACKEND_X86)
#include "evs/x86_backend.h"
#endif
#if defined(EVS_BACKEND_ONNX)
#include "evs/x86_backend.h" // ONNX builds reuse the x86 backend (ORT path)
#endif

namespace evs {

std::shared_ptr<IInferBackend> create_backend(const std::string& type) {
#if defined(EVS_BACKEND_MOCK)
    if (type == "mock" || type.empty()) return std::make_shared<MockBackend>();
#endif
#if defined(EVS_BACKEND_RKNN)
    if (type == "rknn") return std::make_shared<RknnBackend>();
#endif
#if defined(EVS_BACKEND_X86)
    if (type == "x86") return std::make_shared<X86Backend>();
#endif
#if defined(EVS_BACKEND_ONNX)
    // "onnx" and "x86" are the same translation unit; the AND build aliases both
    // so task YAMLs stay portable between the reference and ORT configurations.
    if (type == "onnx" || type == "x86") return std::make_shared<X86Backend>();
#endif
    // qnn backend is compiled only with its SDK present (EVS_BACKEND=qnn).
    (void)type;
    return nullptr;
}

} // namespace evs
