"""Production CLI rejects retired configuration and an unavailable model explicitly."""

from pathlib import Path
import subprocess
import sys
import tempfile

binary, source = [Path(arg).resolve() for arg in sys.argv[1:]]
with tempfile.TemporaryDirectory(prefix="evs_ice_cli_") as directory:
    root = Path(directory)
    image = root / "input.ppm"
    image.write_bytes(b"P6\n2 2\n255\n" + bytes([60] * 12))
    for config in (source / "tests/fixtures/ice_legacy").glob("*.yaml"):
        run = subprocess.run(
            [str(binary), str(config), str(image), str(root / "retired")],
            capture_output=True, text=True,
        )
        assert run.returncode == 2 and "config error" in run.stderr, run.stderr
    config = root / "missing_model.yaml"
    config.write_text((source / "configs/winch_ice.yaml").read_text().replace(
        "Model_path: ice_model.rknn", "Model_path: /no/such/winch.rknn"
    ))
    run = subprocess.run(
        [str(binary), str(config), str(image), str(root / "missing_model")],
        capture_output=True, text=True,
    )
    assert run.returncode == 2 and "initialize error" in run.stderr, run.stderr
    assert not list(root.glob("*_result.json"))
print("CLI PASS: retired configurations and missing model rejected; no result fallback")
