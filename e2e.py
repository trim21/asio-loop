import pathlib
import subprocess
import sys


def run_file(file: pathlib.Path):
    print("running", file.name)
    print("=" * 50)
    with subprocess.Popen([sys.executable, "-X", "faulthandler", str(file)]) as p:
        try:
            code = p.wait(30)
            if code:
                print("failed to execute file", file.name)
                sys.exit(code)
        except subprocess.TimeoutExpired:
            p.terminate()
            print("timeout, failed to execute file", file.name)
            sys.exit(1)
        except Exception as e:
            p.terminate()
            print("failed to execute file", file.name, repr(e))
            sys.exit(1)


def main():
    if len(sys.argv) == 1:
        e2e_dir = pathlib.Path(__file__).parent.joinpath("tests/e2e")
        files = list(e2e_dir.iterdir())
    else:
        files = [pathlib.Path(f) for f in sys.argv[1:]]

    for file in sorted(files, key=lambda p: p.name):
        run_file(file=file)


main()
