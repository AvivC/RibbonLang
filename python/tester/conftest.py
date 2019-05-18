import subprocess
import uuid
import os
import pytest

INTERPRETER_PATH = os.path.join('..', '..', '..', 'plane.exe')
INPUT_PATH = os.path.join('..', '..', 'code.pln')
# INTERPRETER_ARGS = '-asm -tree'
INTERPRETER_ARGS = ''
INTERPRETER_COMMAND = f'{INTERPRETER_PATH} {INPUT_PATH} {INTERPRETER_ARGS}'


@pytest.fixture
def execute():
    def _f(input_text):
        input_file_name = f'{str(uuid.uuid4())}.pln'
        with open(input_file_name, 'w') as f:
            f.write(input_text)

        interpreter_cmd = f'{INTERPRETER_PATH} {input_file_name} {INTERPRETER_ARGS}'
        output = subprocess.run(interpreter_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        os.remove(input_file_name)

        output_text = output.stdout.decode()

        # always assert no memory leaks - kinda ugly, probably refactor this later
        assert "All memory freed" in output_text
        assert "All allocations freed" in output_text

        memory_diagnostics_start = output_text.index('======== Memory diagnostics ========')

        return output_text[:memory_diagnostics_start].strip()  # TODO: .strip() might not be the best idea.
    return _f

#
#
# @pytest.fixture()
# def get_output(request):
#     inputfilename = str(uuid.uuid4())
#     input = request.param
#     with open(inputfilename, 'w') as f:
#         f.write(input)
#
#     interpreter_cmd = f'{INTERPRETER_PATH} {inputfilename} {INTERPRETER_ARGS}'
#     output = subprocess.run(interpreter_cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
#
#     os.remove(inputfilename)
#
#     return output.stdout.decode()
