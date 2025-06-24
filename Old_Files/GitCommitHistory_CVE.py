import os
import subprocess
import re

MAX_SIZE = 3.5 * 1024 * 1024
OUTPUT_DIR = "cve_commit_diffs"
os.makedirs(OUTPUT_DIR, exist_ok=True)

def get_commit_hashes():
    result = subprocess.run(["git", "rev-list", "--reverse", "HEAD"], stdout=subprocess.PIPE, text=True)
    return result.stdout.strip().split('\n')

def commit_has_cve(commit_hash):
    result = subprocess.run(["git", "show", "-s", "--format=%B", commit_hash],
                            stdout=subprocess.PIPE, text=True)
    return re.search(r'\bCVE[- ]?\d{4}-\d{4,7}\b', result.stdout, re.IGNORECASE)

def get_diff(commit_hash):
    result = subprocess.run(["git", "show", commit_hash, "--no-color"], stdout=subprocess.PIPE, text=True)
    return result.stdout

def save_chunks(commit_hashes):
    part = 1
    buffer = ""
    for commit in commit_hashes:
        if not commit_has_cve(commit):
            continue
        diff = get_diff(commit)
        if len(buffer.encode("utf-8")) + len(diff.encode("utf-8")) > MAX_SIZE:
            filename = os.path.join(OUTPUT_DIR, f"diff_part_{part:03d}.txt")
            with open(filename, "w") as f:
                f.write(buffer)
            print(f"Saved {filename}")
            buffer = ""
            part += 1
        buffer += f"\n--- COMMIT {commit} ---\n{diff}\n"
    if buffer:
        filename = os.path.join(OUTPUT_DIR, f"diff_part_{part:03d}.txt")
        with open(filename, "w") as f:
            f.write(buffer)
        print(f"Saved {filename}")

if __name__ == "__main__":
    hashes = get_commit_hashes()
    save_chunks(hashes)
