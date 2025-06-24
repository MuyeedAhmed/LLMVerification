import subprocess
from openpyxl import Workbook
from datetime import datetime

repo_path = "FFmpeg"
output_xlsx = "ExcelFiles/Commits_Jun24-Jun25.xlsx"
before_date = "2024-09-01"
since_date = "2022-02-01"
until_date = datetime.today().strftime("%Y-%m-%d")



log_result = subprocess.run(
    # ["git", "log", "--before", before_date, "-n", "100", "--format=%H"], # Before date filter
    ["git", "log", f"--since={since_date}", f"--until={until_date}", "-n", "100", "--format=%H"], # Since and Until date filter
    cwd=repo_path,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
)
print(log_result)


if log_result.returncode != 0:
    print("Error fetching commits:", log_result.stderr)
    exit(1)

commit_hashes = log_result.stdout.strip().splitlines()

filtered_commits = []

for commit in commit_hashes:
    message_result = subprocess.run(
        ["git", "log", "-1", "--format=%s", commit],
        cwd=repo_path,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    if message_result.returncode != 0:
        print(f"Skipping {commit} due to message fetch error:", message_result.stderr)
        continue

    message = "Fix Requirement: " + message_result.stdout.strip() + "\nGive me the edited c file as attachment (Rename it <filename>_llm.c). Also show the diff."

    show_result = subprocess.run(
        ["git", "show", "--name-only", "--format=", commit],
        cwd=repo_path,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    if show_result.returncode != 0:
        print(f"Skipping {commit} due to file list error:", show_result.stderr)
        continue

    changed_files = list(filter(None, show_result.stdout.strip().splitlines()))
    if len(changed_files) == 1:
        filtered_commits.append((commit, message, changed_files[0]))

wb = Workbook()
ws = wb.active
ws.title = "Commits"
ws.append(["Commit Hash", "Commit Message", "Changed File"])

for commit, message, file in filtered_commits:
    ws.append([commit, message, file])

wb.save(output_xlsx)
