import pathlib
import subprocess
import sys

e2e_dir = pathlib.Path(__file__).parent.joinpath("tests/e2e")

for file in e2e_dir.iterdir():
    subprocess.check_call([sys.executable, str(file)])
