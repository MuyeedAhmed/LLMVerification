import subprocess
import pandas as pd
from pathlib import Path

INPUT_FILE = "ExcelFiles/Commits_Jun24-Jun25.xlsx"
OUTPUT_FILE = "ExcelFiles/Commits_Jun24-Jun25.xlsx"
COMMIT_HASH_COLUMN = "Commit Hash"
GIT_REPO_PATH = "FFmpeg"

def get_commit_date(commit_hash):
    try:
        result = subprocess.run(
            ["git", "-C", GIT_REPO_PATH, "show", "-s", "--format=%cd", "--date=short", commit_hash],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
            text=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None

def main():
    if not Path(INPUT_FILE).exists():
        print(f"File {INPUT_FILE} not found.")
        return

    df = pd.read_excel(INPUT_FILE)

    if COMMIT_HASH_COLUMN not in df.columns:
        print(f"Column '{COMMIT_HASH_COLUMN}' not found in {INPUT_FILE}.")
        return

    print("Fetching commit dates from repo:", GIT_REPO_PATH)

    df["Commit Date"] = df[COMMIT_HASH_COLUMN].apply(get_commit_date)

    df.to_excel(OUTPUT_FILE, index=False)
    print(f"Done. Output written to {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
