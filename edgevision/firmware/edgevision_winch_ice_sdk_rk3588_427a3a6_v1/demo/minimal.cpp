#include <evs/winch_ice.h>
#include <cstdio>
#include <exception>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s model.rknn demo/sample.ppm\n", argv[0]);
        return 2;
    }
    try {
        // 只读取本包的 704x576 PPM 样例。接相机时，用自己的 RGB 缓冲区填写 ImageData。
        constexpr uint32_t width = 704, height = 576;
        std::ifstream input(argv[2], std::ios::binary);
        std::string first, second, third;
        if (!std::getline(input, first) || !std::getline(input, second) ||
            !std::getline(input, third) || first != "P6" || second != "704 576" || third != "255") {
            std::fprintf(stderr, "expected the SDK's 704x576 demo/sample.ppm\n");
            return 2;
        }
        std::vector<uint8_t> rgb(size_t(width) * height * 3);
        if (!input.read(reinterpret_cast<char*>(rgb.data()), rgb.size())) {
            std::fprintf(stderr, "sample image is incomplete\n");
            return 2;
        }

        using namespace evs::winch_ice;
        WinchSnowConfig config;
        config.model_config.Model_path = argv[1];
        config.model_config.input_width = 640;
        config.model_config.input_height = 640;
        config.camera_channel = 0;
        // 三个检测阈值使用默认值 0.45、0.5、0.5；不需要 YAML 或 load_config。
        Detector detector;
        auto status = detector.initialize(config);
        if (status != 0) {
            std::fprintf(stderr, "initialize failed: 0x%08X\n", static_cast<unsigned>(status));
            return 1;
        }

        ImageData image;
        image.data = rgb.data();
        image.width = width;
        image.height = height;
        image.channels = 3; // 紧凑 RGB24，缓冲区至少保持到 detect 返回。
        WinchSnowResult result;
        status = detector.detect(image, result);
        if (status != 0) {
            std::fprintf(stderr, "detect failed: 0x%08X\n", static_cast<unsigned>(status));
            detector.release();
            return 1; // 失败时不读取业务结果。
        }

        std::printf("result_code=%d alert_level=%d winch_candidates=%zu objects=%zu\n",
                    static_cast<int>(result.result_code), static_cast<int>(result.alert_level),
                    result.snow_detections.size(), result.objects.size());
        // snow_detections 已返回完整候选；此例只打印数量和筛选后的卷扬对象。
        for (size_t i = 0; i < result.objects.size(); ++i) {
            const auto& object = result.objects[i];
            const auto& box = object.box;
            std::printf("object[%zu] class_id=%u confidence=%.9g x=%u y=%u width=%u height=%u valid=%d",
                        i, box.class_id, double(box.confidence), box.x, box.y, box.width, box.height,
                        int(object.valid));
            if (object.valid)
                std::printf(" ice_ratio=%.9g\n", double(object.ice_ratio));
            else
                std::printf(" ice_ratio=unknown\n"); // 无效时库内为 -1，不按 0% 处理。
        }
        detector.release();
        return 0;
    } catch (const std::exception& error) {
        // Detector 析构会清理资源；此处处理示例读图、分配等异常。
        std::fprintf(stderr, "%s\n", error.what());
        return 2;
    }
}
