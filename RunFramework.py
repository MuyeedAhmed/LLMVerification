from ExtractGitInfo import ExtractGitInfo
from openpyxl import Workbook
import os
import pandas as pd


from LLM import RunLLM
from MergeOutput import MergeLLMOutput
from RunClang import RunClang


def GetCommitInfo(gitInfo, commit, FileStorePath, filtered_commits):
    message = gitInfo.GetCommitMessages(commit)
    if message is None:
        return
    commit_date = gitInfo.GetCommitDate(commit)
    changed_files = gitInfo.GetChangedFiles(commit)
    diff, changed_functions = gitInfo.GetChangedFunctions(commit)
    changed_function = ",".join(changed_functions)
    prompt = f"Modify the function in the provided C file such that it fixes the following issue: {message} in {changed_function}\nGive me the edited function."
    line_changes = gitInfo.GetTotalLineChanges(commit)

    if line_changes > 15:
        # print(f"Commit {commit} has {line_changes} line changes, skipping commit.")
        return
    if len(changed_functions) != 1:
        # print(f"Commit {commit} has {len(changed_functions)} changed functions.")
        return
    if len(message.split(" ")) < 10:
        # print(f"Commit message for {commit} is too short, skipping commit.")
        return

    if len(changed_files) != 1:
        # print(f"Multiple changed files detected for commit {commit}: {changed_files}")
        return
    if ".c" not in changed_files[0]:
        # print(f"Changed file {changed_files[0]} is not a C file, skipping commit {commit}.")
        return
    
    filtered_commits.append((commit, commit_date, message, prompt, changed_files[0], changed_function, line_changes))

    dest_dir = os.path.join(FileStorePath, commit)
    os.makedirs(dest_dir, exist_ok=True)
    gitInfo.CopyChangedFilesFromCommit(commit, dest_dir)
    gitInfo.CopyChangedFilesFromParent(commit, dest_dir)

    gitInfo.gitCheckoutMaster()


def GetGitInfo(github_link, project, start_date=None, end_date=None, commit_hash = None, commit_count=1):
    projects_dir = "Projects"
    repo_path = os.path.join(projects_dir, project)
    os.makedirs(projects_dir, exist_ok=True)
    os.makedirs("ExcelFiles", exist_ok=True)
    os.makedirs("FileHistory", exist_ok=True)
    FileStorePath = "FileHistory/"+ project
    os.makedirs(FileStorePath, exist_ok=True)
    gitInfo = ExtractGitInfo(github_link, repo_path)
    if not os.path.exists(repo_path):
        gitInfo.gitCloneRepo()
    else:
        gitInfo.gitCheckoutMaster()
    
    filtered_commits = []  
    
    if commit_hash is not None: 
        GetCommitInfo(gitInfo, commit_hash, FileStorePath, filtered_commits)
    else:
        commit_hashes = gitInfo.GetCommitsByDate(start_date, end_date, commit_count)
        for commit in commit_hashes:
            GetCommitInfo(gitInfo, commit, FileStorePath, filtered_commits)

    wb = Workbook()
    ws = wb.active 
    ws.title = "Commits"
    ws.append(["Commit Hash", "Commit Date", "Commit Message", "Prompt", "Changed File", "Changed Functions", "Line Changes"])
    
    if os.path.exists(f"ExcelFiles/{project}.xlsx"):
        existing_df = pd.read_excel(f"ExcelFiles/{project}.xlsx", sheet_name="Commits")
        combined_df = pd.concat([existing_df, pd.DataFrame(filtered_commits, columns=["Commit Hash", "Commit Date", "Commit Message", "Prompt", "Changed File", "Changed Functions", "Line Changes"])])
        combined_df.to_excel(f"ExcelFiles/{project}.xlsx", index=False, sheet_name="Commits")
    else:
        for commit, commit_date, message, prompt, file, changed_function, line_changes in filtered_commits:
            ws.append([commit, commit_date, message, prompt, file, changed_function, line_changes])

        wb.save(f"ExcelFiles/{project}.xlsx")

if __name__ == "__main__":
    github_link = "https://github.com/FFmpeg/FFmpeg.git"
    project = "FFmpeg"

    GetGitInfo(github_link, project, start_date="2024-01-01", end_date="2024-04-30", commit_count=5000)

    AllCommits = pd.read_excel(f"ExcelFiles/{project}.xlsx", sheet_name="Commits")
    for commit_hash in AllCommits["Commit Hash"].values:
        commit_df = AllCommits[AllCommits["Commit Hash"] == commit_hash]
        message = commit_df["Commit Message"].values[0]
        change_file_dir = commit_df["Changed File"].values[0]
        changed_function = commit_df["Changed Functions"].values[0]
        print(f"Processing commit {commit_hash} with message: {message}")
        RunLLM(project, commit_hash, message, changed_function, change_file_dir)

        MergeLLMOutput(project, commit_hash, change_file_dir, changed_function)

        RunClang(project, commit_hash, change_file_dir)


