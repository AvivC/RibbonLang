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


def _parse_test_lines(lines_iter, end_token):
    expect_lines = []
    line = next(lines_iter)
    while line.strip() != end_token:
        if line == '\n':  # allow a special case of bare empty lines, for convenience
            indent = 0
        elif line.startswith('\t'):
            indent = 1
        elif line.startswith('    '):
            indent = 4
        else:
            print()
            print('Parsing error: Indentation missing in test code or expected result.')
            return None

        line = line[indent:]  # deindent
        expect_lines.append(line)
        line = next(lines_iter)

    return ''.join(expect_lines)


def _run_test_file(absolute_path):
    with open(absolute_path) as f:
        text = f.readlines()

    all_success = True

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

        print('Test %-50s' % test_name, end='')

        test_code = _parse_test_lines(lines, 'expect')
        if test_code is None:
            all_success = False
            break

        expect_output = _parse_test_lines(lines, 'end')
        if expect_output is None:
            all_success = False
            break

        interpreter_path = _relative_path_to_abs(os.path.join('..', '..', INTERPRETER_NAME))
        output = _run_on_interpreter(interpreter_path, test_code)
        if output == expect_output:
            print(f'SUCCESS')
        else:
            all_success = False
            print(f'FAILURE')
            print('=== Expected ===')
            print(expect_output)
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
            if not _run_test_file(abs_test_path):
                all_success = False
            print()
    return all_success


def main():
    if len(sys.argv) == 2:
        testdir = sys.argv[1]
    else:
        testdir = DEFAULT_TESTS_DIR

    all_success = _run_test_directory(testdir)

    if all_success:
        print('All tests successful')
        exit(0)
    else:
        print('Some tests failed!')
        exit(-1)


if __name__ == '__main__':
    main()
