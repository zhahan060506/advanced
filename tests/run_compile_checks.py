"""Evaluate hardware-independent C++ regression assertions with the ESP32 compiler.

Run in an ESP-IDF terminal: python tests/run_compile_checks.py
This is compile-time verification, not hardware execution or RTOS simulation.
"""
import argparse
import shutil
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='xtensa-esp32s3-elf-g++')
args = parser.parse_args()
compiler = shutil.which(args.cxx)
if not compiler:
    parser.error('C++ compiler not found; open an ESP-IDF terminal or pass --cxx')
root = Path(__file__).resolve().parents[1]
tests = sorted((root / 'tests').glob('*_test.cc'))
for test in tests:
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsyntax-only', '-I', str(root / 'main'), str(test)], check=True)
    print(f'PASS (compile-time): {test.name}', flush=True)
print(f'{len(tests)} C++ regression suites passed. No hardware execution performed.')
