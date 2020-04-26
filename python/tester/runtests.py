import sys
import os
import subprocess
import uuid
import itertools


TEST_FILE_SUFFIX = '.test'
DEFAULT_TESTS_DIR = 'tests'
INTERPRETER_NAME = 'plane.exe'
INPUT_PATH = os.path.join('..', '..', 'code.pln')
INTERPRETER_ARGS = ''


def _run_on_interpreter(interpreter_path, input_text, additional_files):
    input_file_name = f'{str(uuid.uuid4())}.pln'
    with open(input_file_name, 'w') as f:
        f.write(input_text)

    try:
        for file_name, file_text in additional_files.items():
            file_path = _relative_path_to_abs(os.path.join('..', '..', file_name))
            with open(file_path, 'w') as f:
                f.write(file_text)

        interpreter_cmd = f'{interpreter_path} {input_file_name} {INTERPRETER_ARGS}'
        output = subprocess.run(interpreter_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    finally:
        os.remove(input_file_name)
        for additional_file_name in additional_files:
            os.remove(additional_file_name)

    output_text = output.stdout.decode().replace('\r\n', '\n')

    # always assert no memory leaks - kinda ugly, probably refactor this later
    assert "All memory freed" in output_text
    assert "All allocations freed" in output_text
    assert "No live objects" in output_text

    memory_diagnostics_start = output_text.index('======== Memory diagnostics ========')

    return output_text[:memory_diagnostics_start]


def _parse_test_lines(lines_iter, end_tokens):
    lines = []
    line = next(lines_iter)
    while line.strip() not in end_tokens:
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
        lines.append(line)
        line = next(lines_iter)

    return ''.join(lines)


def _run_test_file(absolute_path):
    with open(absolute_path) as f:
        text = f.readlines()

    all_success = True
    num_tests = 0

    lines = iter(text)
    while True:
        try:
            line = next(lines)
        except StopIteration:
            break

        while line.isspace():
            line = next(lines)

        if not 'test ' in line:
            raise RuntimeError('Text outside test bounds')

        test_prefix, test_name = [s.strip() for s in line.split('test ')]
        
        test_annotations = [s.strip() for s in test_prefix.split()]
        
        for annotation in test_annotations:
            if annotation not in ['repeat', 'skip']:
                raise RuntimeError('Unknown test annotation: {}'.format(annotation))

        #if test_prefix == 'skip':
        if 'skip' in test_annotations:
            while line.strip() != 'end':
                line = next(lines)
            try:
                line = next(lines)  # skip 'end'
            except StopIteration:
                # This was the last test
                pass
            print('Test %-75s [SKIPPED]' % test_name)
            continue
        
        repeat = 'repeat' in test_annotations

        print('Test %-77s' % test_name, end='')

        num_tests += 1

        test_lines = []
        line = next(lines)
        while line.strip() != 'expect' and not line.strip().startswith('file '):
            if line == '\n':  # allow a special case of bare empty lines, for convenience
                indent = 0
            elif line.startswith('\t'):
                indent = 1
            elif line.startswith('    '):
                indent = 4
            else:
                print()
                print('Parsing error: Indentation missing in test code or expected result.')
                test_code = None
                break

            line = line[indent:]  # deindent
            test_lines.append(line)
            line = next(lines)

        else:
            test_code = ''.join(test_lines)

        if test_code is None:
            all_success = False
            break

        additional_files = {}
        while line.strip().startswith('file '):
            file_name = line.split(' ')[1].strip()

            additional_file_lines = []
            line = next(lines)
            while line.strip() != 'expect' and not line.strip().startswith('file '):
                if line == '\n':  # allow a special case of bare empty lines, for convenience
                    indent = 0
                elif line.startswith('\t'):
                    indent = 1
                elif line.startswith('    '):
                    indent = 4
                else:
                    print()
                    print('Parsing error: Indentation missing in test code or expected result.')
                    additional_file_text = None
                    break

                line = line[indent:]  # deindent
                additional_file_lines.append(line)
                line = next(lines)

            else:
                additional_file_text = ''.join(additional_file_lines)

                # The ending '\n' of the file text is not considered part of the file,
                # but part of the test syntax itself, so we remove it
                if additional_file_text[-1] != '\n':
                    raise AssertionError("Additional file text should always end with a '\\n'")
                additional_file_text = additional_file_text[:-1]

            additional_files[file_name] = additional_file_text

        expect_output = _parse_test_lines(lines, ['end'])
        if expect_output is None:
            all_success = False
            break

        interpreter_path = _relative_path_to_abs(os.path.join('..', '..', INTERPRETER_NAME))
        
        if repeat:
            success = True
            for i in range(1000):
                output = _run_on_interpreter(interpreter_path, test_code, additional_files)
                if output != expect_output:
                    success = False
                    failure_repeat = i
                    break
                    
        else:
            output = _run_on_interpreter(interpreter_path, test_code, additional_files)
            success = output == expect_output
            
        if success:
            print(f'SUCCESS')
        else:
            all_success = False
            if repeat:
                print('FAILURE [repeat #{}]'.format(failure_repeat))
            else:
                print(f'FAILURE')
            print()
            print('Ln. %-14s  | Actual' % 'Expected')
            print()
            for i, (expected_line, actual_line) in enumerate(itertools.zip_longest(expect_output.splitlines(), output.splitlines())):
                if expected_line is None:
                    expected_line = '<no more lines>'
                if actual_line is None:
                    actual_line = '<no more lines>'
                print('%-3d %-15s | %-15s' % (i, expected_line, actual_line), end='')
                if expected_line != actual_line:
                    print('[DIFF]')
                else:
                    print()

    return all_success, num_tests


def _relative_path_to_abs(path):
    current_abs_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(current_abs_dir, path)


def _run_test_directory(relative_dir_path):
    num_tests = 0
    all_success = True
    abs_dir_path = _relative_path_to_abs(relative_dir_path)
    for f in os.listdir(abs_dir_path):
        abs_test_path = os.path.join(abs_dir_path, f)
        if os.path.isfile(abs_test_path) and abs_test_path.endswith(TEST_FILE_SUFFIX):
            print(f'Running tests in {f}')
            print()
            file_all_success, num_tests_in_file = _run_test_file(abs_test_path)
            num_tests += num_tests_in_file
            if not file_all_success:
                all_success = False
            print()
    return all_success, num_tests


def main():
    if len(sys.argv) == 2:
        testdir = sys.argv[1]
    else:
        testdir = DEFAULT_TESTS_DIR

    all_success, num_tests = _run_test_directory(testdir)

    print(f'Found total {num_tests} tests.')

    if all_success:
        print('All tests successful')
        exit(0)
    else:
        print('Some tests failed!')
        exit(-1)


if __name__ == '__main__':
    main()
