import pathlib
import subprocess
import sys

e2e_dir = pathlib.Path(__file__).parent.joinpath("tests/e2e")

for file in sorted(e2e_dir.iterdir(), key=lambda p: p.name):
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
