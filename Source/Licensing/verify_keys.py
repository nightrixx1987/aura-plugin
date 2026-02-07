#!/usr/bin/env python3
"""Verify XOR fragments match the Python secret and generate test keys."""
import hashlib
import platform
import subprocess
import re

# === XOR Fragment Verification ===
f1 = [0xe6, 0xd2, 0xf5, 0xc6, 0xf8, 0xe2, 0xd6, 0xf8, 0x95]
f2 = [0x6b, 0x69, 0x6d, 0x04, 0x17, 0x32, 0x18, 0x3e, 0x15]
f3 = [0xa0, 0x96, 0x8c, 0xb8, 0x96, 0xaa, 0x8c, 0x80, 0xb6]
f4 = [0xcc, 0xfd, 0xca, 0xfb, 0xd0, 0xd9, 0xbd]
k1, k2, k3, k4 = 0xa7, 0x5b, 0xd3, 0x8f

cpp_secret = ""
for b in f1: cpp_secret += chr(b ^ k1)
for b in f2: cpp_secret += chr(b ^ k2)
for b in f3: cpp_secret += chr(b ^ k3)
for b in f4: cpp_secret += chr(b ^ k4)

py_secret = "AuRa_Eq_2026_LiCeNsE_kEy_SeCrEt_V2"

print(f"C++ Secret: '{cpp_secret}' ({len(cpp_secret)} chars)")
print(f"PY  Secret: '{py_secret}' ({len(py_secret)} chars)")
print(f"Match: {cpp_secret == py_secret}")
print()

# === Machine ID Generation ===
computer_name = platform.node()
result = subprocess.run(["cmd", "/c", "vol", "C:"], capture_output=True, text=True)
match = re.search(r"([0-9A-Fa-f]{4})-([0-9A-Fa-f]{4})", result.stdout)
serial_num = int(match.group(1) + match.group(2), 16) if match else 0

fingerprint = f"{computer_name}|{serial_num}|AuRa_HW_v2"
machine_id = hashlib.md5(fingerprint.encode("utf-8")).hexdigest()[:8].upper()
print(f"Computer: {computer_name}")
print(f"Volume Serial: {serial_num}")
print(f"Fingerprint: '{fingerprint}'")
print(f"Machine ID: {machine_id}")
print()

# === Generate Test Keys ===
print("Test-Keys fuer diese Machine:")
for i in range(1, 6):
    cid = f"{i:04d}"
    inp = f"{py_secret}-{cid}-{machine_id}"
    sig = hashlib.md5(inp.encode("utf-8")).hexdigest()[:8].upper()
    key = f"AURA-{cid}-{machine_id[:4]}-{sig}"
    print(f"  Key #{i}: {key}")
