import subprocess

url = "wttr.in/Wuhan?format=%t+%C"
res = subprocess.run(["curl", "-s", "-m", "5", url], capture_output=True, text=True)
print(f"returncode: {res.returncode}")
print(f"stdout: {repr(res.stdout)}")
print(f"stderr: {repr(res.stderr)}")
