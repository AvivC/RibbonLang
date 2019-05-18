from os import path
import subprocess

INTERPRETER_PATH = path.join('..', '..', 'plane.exe')
INPUT_PATH = path.join('..', '..', 'code.pln')
INTERPRETER_ARGS = '-asm -tree'
INTERPRETER_COMMAND = f'{INTERPRETER_PATH} {INPUT_PATH} {INTERPRETER_ARGS}'


def main():
    output = subprocess.run(INTERPRETER_COMMAND, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(output.stdout.decode())


if __name__ == '__main__':
    main()
