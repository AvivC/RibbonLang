import sys
import os
import subprocess
import uuid
from collections import namedtuple

TEST_FILE_SUFFIX = '.test'
DEFAULT_TESTS_DIR = 'tests'
INTERPRETER_NAME = 'plane.exe'
INPUT_PATH = os.path.join('..', '..', 'code.pln')
INTERPRETER_ARGS = ''


def _run_on_interpreter(interpreter_path, input_text):
    input_file_name = f'{str(uuid.uuid4())}.pln'
    with open(input_file_name, 'w') as f:
        f.write(input_text)

    try:
        interpreter_cmd = f'{interpreter_path} {input_file_name} {INTERPRETER_ARGS}'
        output = subprocess.run(interpreter_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    finally:
        os.remove(input_file_name)

    output_text = output.stdout.decode().replace('\r\n', '\n')

    # always assert no memory leaks - kinda ugly, probably refactor this later
    assert "All memory freed" in output_text
    assert "All allocations freed" in output_text
    assert "No live objects" in output_text

    memory_diagnostics_start = output_text.index('======== Memory diagnostics ========')

    return output_text[:memory_diagnostics_start]
    # return output_text[:memory_diagnostics_start].strip()  # TODO: .strip() might not be the best idea.


_Test = namedtuple('_Test', ['code', 'expected_output'])


def _run_test_file(absolute_path):
    with open(absolute_path) as f:
        text = f.readlines()

    tests = {}

    lines = iter(text)
    while True:
        try:
            line = next(lines)
        except StopIteration:
            break

        while line.isspace():
            line = next(lines)

        if not line.startswith('test '):
            raise RuntimeError('Text outside test bounds')

        _, test_name = [s.strip() for s in line.split('test ')]

        test_lines = []
        line = next(lines)
        while line.strip() != 'expect':
            test_lines.append(line)
            line = next(lines)
        test_code = ''.join(test_lines)

        expect_lines = []
        line = next(lines)
        while line.strip() != 'end':
            expect_lines.append(line)
            line = next(lines)
        expect_output = ''.join(expect_lines)

        if test_name in tests:
            raise RuntimeError('Duplicate test names')

        tests[test_name] = _Test(test_code, expect_output)

    interpreter_path = _relative_path_to_abs(os.path.join('..', '..', INTERPRETER_NAME))

    all_success = True

    for test_name, test in tests.items():
        output = _run_on_interpreter(interpreter_path, test.code)
        if output == test.expected_output:
            print(f'Test %-35s   SUCCESS' % test_name)
        else:
            all_success = False
            print(f'Test %-35s   FAILURE' % test_name)
            print('=== Expected ===')
            print(test.expected_output)
            print('==============')
            print('=== Actual ===')
            print(output)
            print('==============')

    return all_success


def _relative_path_to_abs(path):
    current_abs_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(current_abs_dir, path)


def _run_test_directory(relative_dir_path):
    all_success = True
    abs_dir_path = _relative_path_to_abs(relative_dir_path)
    for f in os.listdir(abs_dir_path):
        abs_test_path = os.path.join(abs_dir_path, f)
        if os.path.isfile(abs_test_path) and abs_test_path.endswith(TEST_FILE_SUFFIX):
            print(f'Running tests in {f}')
            print()
            all_success = _run_test_file(abs_test_path)
            print()
    return all_success


def main():
    if len(sys.argv) == 2:
        testdir = sys.argv[1]
    else:
        testdir = DEFAULT_TESTS_DIR

    all_success = _run_test_directory(testdir)

    if all_success:
        exit(0)
    else:
        exit(-1)


if __name__ == '__main__':
    main()
