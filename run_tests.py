#!/usr/bin/python3

import subprocess

tests = [
	{"file_name": "tests/fibonacci.k",   "exit_code": 89,  "output": None},
	{"file_name": "tests/small_array.k", "exit_code": 111, "output": None},
	{"file_name": "tests/stack_args.k",  "exit_code": 36,  "output": None},
	{"file_name": "tests/memory_arg.k",  "exit_code": 15,  "output": None},
	{"file_name": "tests/bit_shift.k",   "exit_code": 72, "output": None},
]

def run_test(file_name, exit_code, output):
	print(f"Running test '{file_name}' ... ", end="")
	result = subprocess.run(["./kc", file_name], capture_output=True)
	if result.returncode != 0:
		print("FAILED at compilation.")
		return False
	result = subprocess.run(["./a.out"], capture_output=True)
	if exit_code is not None and exit_code != result.returncode:
		print(f"FAILED expected exit code {exit_code} but got {result.returncode}.")
		return False
	if output is not None and output != result.stdout:
		print(f"FAILED unexpected stdout.")
		return False
	print("PASSED.")
	return True

if __name__ == '__main__':
	passed = 0
	failed = 0
	for test in tests:
		if run_test(**test):
			passed += 1
		else:
			failed += 1
	print()
	print(f"{passed} PASSED")
	print(f"{failed} FAILED")
