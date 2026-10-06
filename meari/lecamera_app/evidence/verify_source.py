#!/usr/bin/env python3
"""Read-only snapshot audit; host probes use extracted functions and fake HAL data.

Usage: python3 evidence/verify_source.py [source_root] > evidence/verification.json
No device, network, production credential, or real Flash access is performed.
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path


ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else
            "/home/tronlong/lyp/meari/lecamera_app").resolve()


def function(path, signature):
    source = (ROOT / path).read_text(errors="replace")
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[start:pos + 1]
    raise ValueError("unterminated function: " + signature)


COMMON = """
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef int pps_s32;
typedef unsigned int pps_u32;
typedef unsigned long pps_ulong;
typedef char pps_char;
#define PPS_ERROR(...) ((void)0)
#define PPS_WARN(...) ((void)0)
#define PPS_INFO(...) ((void)0)
"""


def host_probe(name, extracted, prelude, main):
    compiler = shutil.which("cc")
    if not compiler:
        raise RuntimeError("cc is required for the isolated host probes")
    with tempfile.TemporaryDirectory(prefix="lecamera-doc-probe-", dir="/tmp") as tmp:
        path = Path(tmp) / (name + ".c")
        executable = Path(tmp) / name
        path.write_text(COMMON + prelude + extracted + main)
        subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra",
                        "-Wno-unused-parameter", str(path), "-o", str(executable)],
                       capture_output=True, text=True, check=True, timeout=30)
        result = subprocess.run([str(executable)], capture_output=True,
                                text=True, check=True, timeout=5)
    return {"function_sha256": hashlib.sha256(extracted.encode()).hexdigest(),
            "compiler": compiler, "output": result.stdout.splitlines()}


files = sorted(p for p in ROOT.rglob("*") if p.is_file())
tree_hash = hashlib.sha256()
for path in files:
    tree_hash.update(path.relative_to(ROOT).as_posix().encode() + b"\0"
                     + hashlib.sha256(path.read_bytes()).digest())
report = {"source_root": str(ROOT), "tree_sha256": tree_hash.hexdigest(),
          "file_count": len(files), "suffix_counts": dict(Counter(p.suffix for p in files))}
report["directories"] = {}
for directory in sorted(p for p in ROOT.iterdir() if p.is_dir()):
    members = [p for p in files if directory in p.parents]
    report["directories"][directory.name] = {
        "files": len(members), "c": sum(p.suffix == ".c" for p in members),
        "h": sum(p.suffix == ".h" for p in members)}
report["lexical_counts"] = {}
for scope in ("all", "apps"):
    members = [p for p in files if p.suffix in (".c", ".h", ".cc")
               and (scope == "all" or p.relative_to(ROOT).parts[0] == scope)]
    texts = [p.read_text(errors="replace") for p in members]
    report["lexical_counts"][scope] = {
        name: sum(len(re.findall(r"\b" + name + r"\s*\(", s)) for s in texts)
        for name in ("strcpy", "sprintf", "strcat", "snprintf", "malloc", "free")}
report["bundled_a_so_files"] = [str(p.relative_to(ROOT)) for p in files
                                 if p.suffix in (".a", ".so")]

report["make_dry_runs"] = {}
for platform, factory in (("b8", "neutral_nkit"), ("b7", "neutral_std"),
                          ("b6", "neutral_std"), ("b9", "neutral_std"),
                          ("x86", "neutral_std")):
    command = ["make", "-n", "VCAMERA_PLATFORM_TYPE=" + platform,
               "VCAMERA_FACTORY_TYPE=" + factory,
               "VCAMERA_PLATFORM_ID=" + platform + "-" + factory,
               "VCAMERA_BUILD_DIR=/tmp/lecamera-doc-dry/" + platform,
               "VCAMERA_BUILD_BRANCH=meari", "VCAMERA_BUILD_TYPE=release"]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=30)
    report["make_dry_runs"][platform] = {
        "command": command, "exit_code": result.returncode,
        "stderr": result.stderr.splitlines(),
        "stdout_sha256": hashlib.sha256(result.stdout.encode()).hexdigest()}
report["mips_compiler_in_path"] = shutil.which("mips-linux-uclibc-gnu-gcc")

user_header = (ROOT / "device/pps_device_user.h").read_text()
user_constants = "\n".join(re.findall(r"^#define PPS_MAX_\w+\s+\d+", user_header, re.M))
report["host_probes"] = {}
report["host_probes"]["user_auth"] = host_probe(
    "user_auth", function("device/pps_device_user.c", "int pps_verify_user("),
    user_constants + """
typedef struct { unsigned char username[PPS_MAX_USERNAME];
                 unsigned char password[PPS_MAX_PASSWDLEN]; } USER;
typedef struct { USER user[PPS_MAX_USERNUM]; } dev_cfg_param_t;
static dev_cfg_param_t fake_cfg;
static void dev_cfg_lock(void) {}
static void dev_cfg_unlock(void) {}
static dev_cfg_param_t *get_dev_cfg_param(void) { return &fake_cfg; }
""", """
int main(void) {
    const char *user = "audituser", *password = "examplepass";
    for (size_t i = 0; i < strlen(user); ++i)
        fake_cfg.user[0].username[i] = (unsigned char)~user[i];
    for (size_t i = 0; i < strlen(password); ++i)
        fake_cfg.user[0].password[i] = (unsigned char)~password[i];
    int full = pps_verify_user("audituser", "examplepass");
    int wrong = pps_verify_user("wrong", "wrong");
    int empty = pps_verify_user("", "");
    int prefix = pps_verify_user("a", "e");
    int empty_password = pps_verify_user("audituser", "");
    printf("full=%d wrong=%d empty=%d prefix=%d empty_password=%d\\n",
           full, wrong, empty, prefix, empty_password);
    assert(full == 0 && wrong == -1 && empty == 0 && prefix == 0 && empty_password == 0);
    return 0;
}
""")
report["host_probes"]["ssl_ca_init"] = host_probe(
    "ssl_ca_init", function("core/pps_ssl.c", "pps_ssl_t *pps_ssl_init("), """
typedef struct { char *ca_cert; int ssl_verify_host; int ssl_verify_peer; } pps_ssl_t;
""", """
int main(void) {
    pps_ssl_t *ssl = pps_ssl_init("test-ca-placeholder", 1, 1);
    assert(ssl != NULL);
    printf("ca_saved=%d verify_host=%d verify_peer=%d\\n",
           ssl->ca_cert != NULL, ssl->ssl_verify_host, ssl->ssl_verify_peer);
    assert(ssl->ca_cert == NULL && ssl->ssl_verify_host == 1 && ssl->ssl_verify_peer == 1);
    free(ssl->ca_cert); free(ssl);
    return 0;
}
""")
report["host_probes"]["flash_write_failure"] = host_probe(
    "flash_write_failure", function("device/pps_device_upgrade.c", "static pps_s32 upgrade_boot("), """
#define PPS_ERR_INVALARG (-2)
static int g_write_percent_size, g_upgrade_percent, g_upg_size = 65536;
static int writes, erases, fail_writes;
static int pps_flash_erase(char *f, unsigned long p, unsigned long n) {
    ++erases; return 0;
}
static int pps_flash_write(char *f, char *data, unsigned long p, unsigned long n) {
    ++writes; return fail_writes ? -1 : 0;
}
static int usleep(unsigned int n) { return 0; }
""", """
int main(void) {
    char data[65536] = {0};
    int ok = upgrade_boot(data, "fake-partition", sizeof(data), 0, sizeof(data));
    assert(ok == 0 && writes == 1);
    writes = erases = g_write_percent_size = 0;
    fail_writes = 1;
    int failure = upgrade_boot(data, "fake-partition", sizeof(data), 0, sizeof(data));
    printf("success_return=%d all_writes_fail_return=%d write_attempts=%d erase_attempts=%d\\n",
           ok, failure, writes, erases);
    assert(failure == 0 && writes == 10 && erases == 10);
    return 0;
}
""")
print(json.dumps(report, ensure_ascii=False, indent=2))
