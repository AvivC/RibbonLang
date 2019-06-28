import subprocess
import uuid
import os
from collections import namedtuple

import pytest

# INTERPRETER_PATH = os.path.join('..', '..', '..', 'plane.exe')
INTERPRETER_PATH = os.path.join('..', '..', 'plane.exe')
INPUT_PATH = os.path.join('..', '..', 'code.pln')
INTERPRETER_ARGS = ''
INTERPRETER_COMMAND = f'{INTERPRETER_PATH} {INPUT_PATH} {INTERPRETER_ARGS}'


def _run_on_interpreter(input_text):
    input_file_name = f'{str(uuid.uuid4())}.pln'
    with open(input_file_name, 'w') as f:
        f.write(input_text)

    interpreter_cmd = f'{INTERPRETER_PATH} {input_file_name} {INTERPRETER_ARGS}'
    output = subprocess.run(interpreter_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    os.remove(input_file_name)

    output_text = output.stdout.decode().replace('\r\n', '\n')

    # always assert no memory leaks - kinda ugly, probably refactor this later
    assert "All memory freed" in output_text
    assert "All allocations freed" in output_text

    memory_diagnostics_start = output_text.index('======== Memory diagnostics ========')

    return output_text[:memory_diagnostics_start].strip()  # TODO: .strip() might not be the best idea.


_Test = namedtuple('_Test', ['code', 'expected_output'])


@pytest.fixture
def test_file():
    def _inner(path):
        with open(path) as f:
            text = f.readlines()

        tests = {}

        lines = iter(text)
        while True:
            try:
                line = next(lines)
            except StopIteration:
                return

            if not line.startswith('test '):
                raise RuntimeError('Text outside test bounds')

            _, test_name = line.split('test ')

            test_lines = []
            line = next(lines)
            while line.strip() != 'expect':
                test_lines.append(line)
            test_code = '\n'.join(test_lines)

            expect_lines = []
            line = next(lines)
            while line.strip() != 'end':
                expect_lines.append(line)
            expect_output = '\n'.join(expect_lines)

            tests[test_name] = _Test(test_code, expect_output)

        for test_name, test in tests.items():
            print(test_name)
            output = _run_on_interpreter(test.code)
            assert output == test.expected_output

    return _inner


@pytest.fixture
def execute():
    return _run_on_interpreter


@pytest.fixture
def execprint():
    def _f(input_text):
        return _run_on_interpreter(f'print({input_text})')
    return _f

