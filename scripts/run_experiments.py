#!/usr/bin/env python3

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile

DEFAULT_TARGET = "TARGET_X86_64"
DEFAULT_RUNNER = "RUNNER_USER"
DEFAULT_CYCLES = "1000000000"
ORCH = "./orchestrator"
ANALYZER = "python3 analyzer.py"
SPECTRE_URL = "https://raw.githubusercontent.com/speed47/spectre-meltdown-checker/master/spectre-meltdown-checker.sh"
SPECTRE_PATCH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patch.diff")

FEATURES = [
    "cache", "pht", "rob", "lap", "rsb", "simd", "pipeline",
    "lfb", "smt", "tlb", "sgx", "spec_mem_access", "ooo_mem_access",
    "stl_forward", "stale_code_execution", "btb", "fpu", "locks", "o3",
    "kernel_rsb", "kernel_bti", "kernel_lap", "kernel_lfb",
    "kernel_stl", "kernel_stale_code", "kernel_o3", "kernel_misprediction",
    "kernel_tlb",
    "user_rsb", "user_bti", "user_lap", "user_lfb",
    "user_stl", "user_stale_code", "user_o3", "user_misprediction",
    "user_tlb",
    "process_rsb", "process_stl", "process_stale_code",
    "process_o3", "process_misprediction", "process_lfb",
    "process_lap", "process_bti", "process_tlb",
    "mprotected_access", "kernel_access",
]

ATTACKS = {
    "ridl": ["lfb", "ooo_mem_access", "smt"],
    "cacheout": ["sgx", "smt", "lfb"],
    "fallout": ["stl_forward", "ooo_mem_access", "tlb"],
    "foreshadow": ["stl_forward", "ooo_mem_access", "tlb", "btb"],
    "zombieload": ["smt", "ooo_mem_access", "sgx"],
    "meltdown": ["ooo_mem_access"],
    "lvi": ["ooo_mem_access", "spec_mem_access", "stl_forward", "lfb"],
    "spectre_rsb": ["spec_mem_access", "rsb", "btb"],
    "spectre_stl": ["spec_mem_access", "stl_forward"],
    "spectre_v1": ["spec_mem_access", "pht"],
    "spectre_btb": ["spec_mem_access", "btb"],
    "slap": ["lap", "spec_mem_access"],
    "scsb": ["stale_code_execution", "spec_mem_access"],
}


def build_shell_cmd(name, modules, target, runner, cycles, saved_dir,
                     kernel_headers=None):
    module_string = ",".join(sorted(set(modules)))
    kh_flag = f" -k {kernel_headers}" if kernel_headers else ""
    orch_cmd = (
        f"{ORCH} -t {target} -r {runner} -c {cycles}"
        f"{kh_flag} -m {module_string} -s{saved_dir}/{name} all"
    )
    return f"pushd .. > /dev/null\n{orch_cmd}\npopd > /dev/null"


def build_all_cmd(target, runner, cycles, saved_dir, kernel_headers=None):
    kh_flag = f" -k {kernel_headers}" if kernel_headers else ""
    orch_cmd = (
        f"{ORCH} -t {target} -r {runner} -c {cycles}"
        f"{kh_flag} -s{saved_dir}/all all"
    )
    return f"pushd .. > /dev/null\n{orch_cmd}\npopd > /dev/null"


def run_cmd(shell_cmd, dry):
    print(shell_cmd)
    print()
    if not dry:
        subprocess.run(["bash", "-c", shell_cmd], check=True)


def analyze_dir(dir_path, dry):
    cmd = f"pushd .. > /dev/null\n{ANALYZER} {dir_path} --export\npopd > /dev/null"
    print(cmd)
    print()
    if not dry:
        subprocess.run(["bash", "-c", cmd], check=True)


def save_cpu_info(saved_dir, dry):
    dest = os.path.join(saved_dir, "cpuinfo.txt")
    print(f"\n### SAVING CPU INFO → {dest} ###\n")
    if not dry:
        try:
            with open("/proc/cpuinfo") as f:
                info = f.read()
            with open(dest, "w") as f:
                f.write(info)
            print(f"Saved {len(info)} bytes")
        except Exception as e:
            print(f"Failed to save cpuinfo: {e}")


def run_spectre_checker(saved_dir, dry):
    print("\n### SPECTRE-MELTDOWN CHECKER ###\n")

    checker_path = shutil.which("spectre-meltdown-checker.sh")
    if not checker_path:
        checker_path = os.path.join(saved_dir, "spectre-meltdown-checker.sh")
        if not os.path.exists(checker_path):
            if dry:
                print(f"[dry-run] Would download from {SPECTRE_URL} to {checker_path}")
            else:
                print(f"Downloading spectre-meltdown-checker.sh ...")
                try:
                    import urllib.request
                    urllib.request.urlretrieve(SPECTRE_URL, checker_path)
                    os.chmod(checker_path, 0o755)
                except Exception as e:
                    print(f"Failed to download checker: {e}")
                    return

    if os.path.exists(SPECTRE_PATCH):
        print(f"Applying patch from {SPECTRE_PATCH} ...")
        if dry:
            print(f"[dry-run] Would run: patch -N -t {checker_path} {SPECTRE_PATCH}")
        else:
            try:
                subprocess.run(
                    ["patch", "-N", "-t", checker_path, SPECTRE_PATCH],
                    capture_output=True, text=True, timeout=30
                )
            except Exception as e:
                print(f"Failed to apply patch: {e}")
                return

    if dry:
        print(f"[dry-run] Would run: sudo {checker_path} --batch json")
        print(f"[dry-run] Would run: sudo {checker_path}")
        print(f"[dry-run] Would run: sudo {checker_path} --vendor-check --no-color --hw-only")
        return

    try:
        result_json = subprocess.run(
            ["sudo", checker_path, "--batch", "json"],
            capture_output=True, text=True, timeout=120
        )
        result_txt = subprocess.run(
            ["sudo", checker_path],
            capture_output=True, text=True, timeout=120
        )
    except FileNotFoundError:
        print(f"Checker script not found at {checker_path}")
        return
    except PermissionError:
        print("Need sudo to run spectre-meltdown-checker")
        return

    json_path = os.path.join(saved_dir, "spectre_checker.json")
    with open(json_path, "w") as f:
        f.write(result_json.stdout)
    print(f"JSON output saved to {json_path}")

    txt_path = os.path.join(saved_dir, "spectre_checker.txt")
    with open(txt_path, "w") as f:
        f.write(result_txt.stdout)
        if result_txt.stderr:
            f.write("\n=== STDERR ===\n")
            f.write(result_txt.stderr)
    print(f"Human-readable output saved to {txt_path}")

    if result_json.stdout:
        try:
            data = json.loads(result_json.stdout)
            summary = data.get("summary", {})
            print("  Vulnerable:   ", summary.get("VULNERABLE", "?"))
            print("  OK (mitigated):", summary.get("OK", "?"))
            print("  Unknown:     ", summary.get("UNKNOWN", "?"))
        except json.JSONDecodeError:
            print("  (could not parse JSON)")

    print("\n### VENDOR CHECK ###\n")
    try:
        result_vendor = subprocess.run(
            ["sudo", checker_path, "--vendor-check", "--no-color", "--hw-only"],
            capture_output=True, text=True, timeout=120
        )
    except FileNotFoundError:
        print(f"Checker script not found at {checker_path}")
        return
    except PermissionError:
        print("Need sudo to run spectre-meltdown-checker")
        return

    vendor_path = os.path.join(saved_dir, "spectre_vendor_check.txt")
    with open(vendor_path, "w") as f:
        f.write(result_vendor.stdout)
        if result_vendor.stderr:
            f.write("\n=== STDERR ===\n")
            f.write(result_vendor.stderr)
    print(f"Vendor check output saved to {vendor_path}")


def run_runner(target, runner, cycles, saved_dir, args):
    run_all = not args.no_all
    run_features = args.features
    run_attacks = args.attacks

    runner_label = runner.replace("RUNNER_", "").lower()
    runner_dir = os.path.join(saved_dir, runner_label)
    os.makedirs(runner_dir, exist_ok=True)

    kh = args.kernel_headers

    if run_all:
        print(f"\n### ALL MODULES ({runner_label}) ###\n")
        cmd = build_all_cmd(target, runner, cycles, runner_dir, kh)
        run_cmd(cmd, args.dry)

    if run_features:
        print(f"\n### FEATURE RUNS ({runner_label}) ###\n")
        for f in FEATURES:
            cmd = build_shell_cmd(f, [f], target, runner, cycles, runner_dir, kh)
            run_cmd(cmd, args.dry)

    if run_attacks:
        print(f"\n### ATTACK RUNS ({runner_label}) ###\n")
        for name, mods in ATTACKS.items():
            cmd = build_shell_cmd(name, mods, target, runner, cycles, runner_dir, kh)
            run_cmd(cmd, args.dry)

    if not args.no_analyze and not args.dry:
        print(f"\n### ANALYSIS WITH EXPORT ({runner_label}) ###\n")
        analyze_dir(runner_dir, args.dry)


def main():
    parser = argparse.ArgumentParser(
        description="Run experiments and auto-analyze"
    )
    parser.add_argument("-t", "--target", default=DEFAULT_TARGET,
                        help="Architecture target")
    parser.add_argument("-r", "--runner", default=DEFAULT_RUNNER,
                        help="Runner type (RUNNER_USER or RUNNER_KERNEL)")
    parser.add_argument("-c", "--cycles", default=DEFAULT_CYCLES,
                        help="Cycle count")
    parser.add_argument("-k", "--kernel-headers",
                        help="Kernel headers directory")
    parser.add_argument("--saved-dir", default="saved",
                        help="Directory to save results (created if missing)")
    parser.add_argument("--dry", action="store_true", help="Print only")
    parser.add_argument("--features", action="store_true",
                        help="Run features individually")
    parser.add_argument("--attacks", action="store_true",
                        help="Run attacks individually")
    parser.add_argument("--no-analyze", action="store_true",
                        help="Skip auto-analysis with export")
    parser.add_argument("--no-all", action="store_true",
                        help="Skip the combined all-module run")
    parser.add_argument("--no-cpuinfo", action="store_true",
                        help="Skip saving /proc/cpuinfo")
    parser.add_argument("--no-spectre-checker", action="store_true",
                        help="Skip running spectre-meltdown-checker")
    parser.add_argument("--run-both", action="store_true",
                        help="Run both USER and KERNEL runners")

    args = parser.parse_args()

    saved_dir = os.path.abspath(args.saved_dir)
    os.makedirs(saved_dir, exist_ok=True)

    if not args.no_cpuinfo:
        save_cpu_info(saved_dir, args.dry)

    if not args.no_spectre_checker:
        run_spectre_checker(saved_dir, args.dry)

    if args.run_both:
        for runner in ("RUNNER_USER", "RUNNER_KERNEL"):
            print(f"\n{'='*60}")
            print(f"RUNNER: {runner}")
            print(f"{'='*60}")
            run_runner(args.target, runner, args.cycles, saved_dir, args)
    else:
        run_runner(args.target, args.runner, args.cycles, saved_dir, args)


if __name__ == "__main__":
    main()
