from ExtractGitInfo import ExtractGitInfo
from openpyxl import Workbook
import os
import pandas as pd


from LLM import RunLLM, MergeLLMOutput
from RunClang import RunClang


def GetGitInfo(github_link, project):
    projects_dir = "Projects"
    repo_path = os.path.join(projects_dir, project)
    os.makedirs(projects_dir, exist_ok=True)
    os.makedirs("ExcelFiles", exist_ok=True)
    os.makedirs("FileHistory", exist_ok=True)
    FileStorePath = "FileHistory"
    run_clang = ExtractGitInfo(github_link, repo_path)
    if not os.path.exists(repo_path):
        run_clang.gitCloneRepo()
    else:
        run_clang.gitCheckoutMaster()
    since_date = "2025-06-22"
    until_date = "2025-06-24"
    commit_count = 10
    commit_hashes = run_clang.GetCommitsByDate(since_date, until_date, commit_count)
    filtered_commits = []  
    for commit in commit_hashes:
        message = run_clang.GetCommitMessages(commit)
        if message is None:
            continue
        commit_date = run_clang.GetCommitDate(commit)
        changed_files = run_clang.GetChangedFiles(commit)
        diff, changed_function = run_clang.GetChangedFunctions(commit)

        changed_function = ",".join(changed_function)
        prompt = f"Fix Requirement: {message}\nFunction:{changed_function}\nGive me the edited function. Also show the diff."
        
        if len(changed_files) == 1:
            filtered_commits.append((commit, commit_date, message, prompt, changed_files[0], changed_function, diff))
        if len(changed_files) > 1:
            print(f"Multiple changed files detected for commit {commit}: {changed_files}")
            continue
        if ".c" not in changed_files[0]:
            print(f"Changed file {changed_files[0]} is not a C file, skipping commit {commit}.")
            continue
        dest_dir = os.path.join(FileStorePath, commit)
        os.makedirs(dest_dir, exist_ok=True)
        run_clang.CopyChangedFilesFromParent(commit, dest_dir)
        run_clang.CopyChangedFilesFromCommit(commit, dest_dir)

        run_clang.gitCheckoutMaster()

    wb = Workbook()
    ws = wb.active 
    ws.title = "Commits"
    ws.append(["Commit Hash", "Commit Date", "Commit Message", "Prompt", "Changed File", "Changed Functions"])
    for commit, commit_date, message, prompt, file, changed_function, diff in filtered_commits:
        ws.append([commit, commit_date, message, prompt, file, changed_function, diff])
    wb.save(f"ExcelFiles/{project}.xlsx")



if __name__ == "__main__":
    github_link = "https://github.com/FFmpeg/FFmpeg.git"
    project = "FFmpeg"

    # GetGitInfo(github_link, project)

    AllCommits = pd.read_excel(f"ExcelFiles/{project}.xlsx", sheet_name="Commits")
    commit_hash = "b587afcb65192c4c4bf88422f6565e5355eaf31e"
    commit_df = AllCommits[AllCommits["Commit Hash"] == commit_hash]
    message = commit_df["Commit Message"].values[0]
    change_file_dir = commit_df["Changed File"].values[0]
    changed_function = commit_df["Changed Functions"].values[0]

    # RunLLM(commit, message, changed_function, change_file_dir)

    # MergeLLMOutput(commit, change_file_dir, changed_function)

    RunClang(project, commit_hash, change_file_dir)