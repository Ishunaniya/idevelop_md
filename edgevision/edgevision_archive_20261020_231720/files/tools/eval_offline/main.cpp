// Offline evaluation tool for EdgeVision.
//
// Validates the detection algorithm on a host (x86) using field photos and
// manual annotations, independently of the embedded device:
//
//   photos/annotations -> Preprocessor -> IInferBackend (x86 reference or ONNX)
//   -> decoder -> match vs ground truth (IoU) -> precision/recall/F1/mAP@0.5
//
// Usage:
//   eval_offline --task configs/eval_demo.yaml --dataset ./fixtures \
//                [--iou 0.5] [--score 0.45] [--dump] [--out ./out]
//
// Writes report.json, report.csv and images.csv into --out (default: dataset).

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "evs/log.h"
#include "evs/offline_eval.h"

namespace fs = std::filesystem;

static bool write_file(const std::string& path, const std::string& content);

static void print_help(const char* argv0) {
    std::cout
        << "EdgeVision offline evaluator\n\n"
        << "Usage:\n"
        << "  " << argv0 << " --task <task.yaml> --dataset <dir>\n"
        << "            [--iou 0.5] [--score 0.45] [--dump] [--out <dir>]\n\n"
        << "  --task     task YAML (backend/model/input dims/decoder)\n"
        << "  --dataset  dataset root containing images/ + annotations.json\n"
        << "  --iou      IoU match threshold (default 0.5)\n"
        << "  --score    detection score threshold (default: from task YAML)\n"
        << "  --dump     dump FP/FN/TP comparison PPMs into --out for inspection\n"
        << "  --out      output directory for report.json / .csv (default: dataset)\n";
}

int main(int argc, char** argv) {
    std::string task, dataset, out_dir;
    float iou = 0.5f;
    float score = -1.f;
    bool dump = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* def) -> std::string {
            return (i + 1 < argc) ? std::string(argv[++i]) : std::string(def);
        };
        if (a == "--help" || a == "-h") { print_help(argv[0]); return 0; }
        else if (a == "--task") task = next("");
        else if (a == "--dataset") dataset = next("");
        else if (a == "--out") out_dir = next("");
        else if (a == "--iou") iou = static_cast<float>(std::atof(next("0.5").c_str()));
        else if (a == "--score") score = static_cast<float>(std::atof(next("0.45").c_str()));
        else if (a == "--dump") dump = true;
        else { std::cerr << "unknown argument: " << a << "\n"; return 2; }
    }

    if (task.empty() || dataset.empty()) {
        print_help(argv[0]);
        return 2;
    }
    if (out_dir.empty()) out_dir = dataset;
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    evs::EvalConfig cfg;
    cfg.task_yaml = task;
    cfg.dataset_dir = dataset;
    cfg.iou_threshold = iou;
    cfg.confidence_threshold = score;
    cfg.dump_mismatch = dump;
    cfg.dump_dir = (fs::path(out_dir) / "dump").string();
    cfg.class_names = "person,helmet";

    evs::OfflineEvaluator eval;
    uint32_t r = eval.load(cfg);
    if (r != 0u) {
        std::cerr << "load failed (status=0x" << std::hex << r << std::dec << ")\n";
        return 1;
    }
    evs::EvalReport rep;
    r = eval.run(rep);
    if (r != 0u) {
        std::cerr << "run failed (status=0x" << std::hex << r << std::dec << ")\n";
        return 1;
    }

    // Console summary.
    std::cout << "=================================================\n";
    std::cout << " EdgeVision offline evaluation\n";
    std::cout << "-------------------------------------------------\n";
    std::cout << " backend         : " << rep.backend << " (" << rep.Model_path << ")\n";
    std::cout << " images          : " << rep.n_images << "\n";
    std::cout << " IoU / score     : " << rep.iou_threshold << " / "
              << rep.confidence_threshold << "\n";
    std::cout << "-------------------------------------------------\n";
    std::cout << " micro P / R     : " << rep.micro_precision << " / "
              << rep.micro_recall << "\n";
    std::cout << " mAP@" << rep.iou_threshold << "        : " << rep.map << "\n";
    std::cout << "-------------------------------------------------\n";
    std::cout << " per-class:  tp  fp  fn    P      R      F1     AP\n";
    for (const auto& kv : rep.per_class) {
        const auto& m = kv.second;
        std::cout << "   class " << kv.first << " : " << m.tp << "  " << m.fp
                  << "  " << m.fn << "  " << m.precision << " " << m.recall
                  << " " << m.f1 << " " << m.ap << "\n";
    }
    std::cout << "=================================================\n";

    // Write artifacts.
    const std::string rj = (fs::path(out_dir) / "report.json").string();
    const std::string rc = (fs::path(out_dir) / "report.csv").string();
    const std::string ic = (fs::path(out_dir) / "images.csv").string();
    if (!write_file(rj, rep.to_json()))
        std::cerr << "warning: cannot write " << rj << "\n";
    if (!rep.write_csv(rc))
        std::cerr << "warning: cannot write " << rc << "\n";
    if (!rep.write_images_csv(ic))
        std::cerr << "warning: cannot write " << ic << "\n";

    std::cout << "reports written to: " << out_dir << "\n";
    return 0;
}

// Small helper kept here to avoid pulling the debug TU into the tool binary's
// public surface; writes a string to a file.
static bool write_file(const std::string& path, const std::string& content) {
    std::ofstream o(path, std::ios::binary);
    if (!o) return false;
    o << content;
    return true;
}
