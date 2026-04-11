#!/usr/bin/python3

import subprocess

fizzbuzz = ("fizzbuzz\n"
			"1\n"
			"2\n"
			"fizz\n"
			"4\n"
			"buzz\n"
			"fizz\n"
			"7\n"
			"8\n"
			"fizz\n"
			"buzz\n"
			"11\n"
			"fizz\n"
			"13\n"
			"14\n"
			"fizzbuzz\n"
			"16\n"
			"17\n"
			"fizz\n"
			"19\n"
			"buzz\n"
			"fizz\n"
			"22\n"
			"23\n"
			"fizz\n"
			"buzz\n"
			"26\n"
			"fizz\n"
			"28\n"
			"29\n"
			"fizzbuzz\n"
			"31\n"
			"32\n"
			"fizz\n"
			"34\n"
			"buzz\n"
			"fizz\n"
			"37\n"
			"38\n"
			"fizz\n"
			"buzz\n"
			"41\n"
			"fizz\n"
			"43\n"
			"44\n"
			"fizzbuzz\n"
			"46\n"
			"47\n"
			"fizz\n"
			"49\n"
			"buzz\n"
			"fizz\n"
			"52\n"
			"53\n"
			"fizz\n"
			"buzz\n"
			"56\n"
			"fizz\n"
			"58\n"
			"59\n"
			"fizzbuzz\n"
			"61\n"
			"62\n"
			"fizz\n"
			"64\n"
			"buzz\n"
			"fizz\n"
			"67\n"
			"68\n"
			"fizz\n"
			"buzz\n"
			"71\n"
			"fizz\n"
			"73\n"
			"74\n"
			"fizzbuzz\n"
			"76\n"
			"77\n"
			"fizz\n"
			"79\n"
			"buzz\n"
			"fizz\n"
			"82\n"
			"83\n"
			"fizz\n"
			"buzz\n"
			"86\n"
			"fizz\n"
			"88\n"
			"89\n"
			"fizzbuzz\n"
			"91\n"
			"92\n"
			"fizz\n"
			"94\n"
			"buzz\n"
			"fizz\n"
			"97\n"
			"98\n"
			"fizz\n")

slice_output = "Hello, World\nHello\nWorld\n0 1 2 3 4 \n0 1 2 \n2 3 4 \n"
constexpr_output = "489\n-351\n28980\n0\n481\n4\nfalse\ntrue\nfalse\ntrue\nfalse\n"
generic_types = "69\n420\n1303948\n38438273\n32 33 34 35 36 37 38 \n69\nHello, World\n690\n6969\n"
simple_union = "argc = 1\nfile = ./a.out\nOpened file: ./a.out\n"



tests = [
	{"file_name": "tests/fibonacci.k",			   "exit_code": 89,  "output": None},
	{"file_name": "tests/small_array.k",		   "exit_code": 114, "output": None},
	{"file_name": "tests/stack_args.k",			   "exit_code": 36,  "output": None},
	{"file_name": "tests/memory_arg.k",			   "exit_code": 15,  "output": None},
	{"file_name": "tests/bit_shift.k",			   "exit_code": 72,  "output": None},
	{"file_name": "tests/large_return_values.k",   "exit_code": 11,  "output": None},
	{"file_name": "tests/hello_world.k",		   "exit_code": 0,   "output": "Hello, World\n"},
	{"file_name": "tests/simple_struct.k",		   "exit_code": 69,  "output": "Hello, World\n"},
	{"file_name": "tests/fizzbuzz.k",		       "exit_code": 0,   "output": fizzbuzz},
	{"file_name": "tests/slice.k",		           "exit_code": 0,   "output": slice_output},
	{"file_name": "tests/constant_expressions.k",  "exit_code": 0,   "output": constexpr_output},
	{"file_name": "tests/polymorphic_procedure.k", "exit_code": 0,   "output": "69\n420\n-348\n"},
	{"file_name": "tests/generic_types.k",         "exit_code": 0,   "output": generic_types},
	{"file_name": "tests/simple_union.k",          "exit_code": 0,   "output": simple_union},
]

def run_test(file_name, exit_code, output):
	print(f'Running test "{file_name}" ... ', end="")
	result = subprocess.run(["./kc", file_name], capture_output=True)
	if result.returncode != 0:
		print("FAILED at compilation.")
		return False
	result = subprocess.run(["./a.out"], capture_output=True)
	if exit_code is not None and exit_code != result.returncode:
		print(f"FAILED expected exit code {exit_code} but got {result.returncode}.")
		return False
	if output is not None and output != result.stdout.decode('UTF-8'):
		print("FAILED unexpected stdout.")
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
