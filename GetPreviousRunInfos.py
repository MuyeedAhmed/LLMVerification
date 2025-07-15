from ExtractGitInfo import ExtractGitInfo
from openpyxl import Workbook
import os
import pandas as pd


def GetCommitInfo(gitInfo, commit):
    message = gitInfo.GetCommitMessages(commit)
    if message is None:
        return
    commit_date = gitInfo.GetCommitDate(commit)
    changed_files = gitInfo.GetChangedFiles(commit)
    diff, changed_functions = gitInfo.GetChangedFunctions(commit)
    changed_function = ",".join(changed_functions)
    line_changes = gitInfo.GetTotalLineChanges(commit)

    # gitInfo.gitCheckoutMaster()
    return message, commit_date, changed_files, changed_function, line_changes

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
    repo_path = os.path.join("Projects", project)
    gitInfo = ExtractGitInfo(github_link, repo_path)

    AllCommits = pd.read_excel(f"ExcelFiles/{project}_Select.xlsx", sheet_name=project)
    AllCommits["Line Changes"] = 0
    AllCommits["Message Len"] = 0
    for commit_hash in AllCommits["Commit Hash"].values:
        row_idx = AllCommits["Commit Hash"] == commit_hash

        message = AllCommits.loc[row_idx, "Commit Description"].values[0]
        AllCommits.loc[row_idx, "Message Len"] = len(message.split(" "))

        message, commit_date, changed_files, changed_function, line_changes = GetCommitInfo(gitInfo, commit_hash)
        AllCommits.loc[row_idx, "Line Changes"] = line_changes
        AllCommits.loc[row_idx, "Functions Changed"] = changed_function

    AllCommits.to_excel(f"ExcelFiles/{project}_Select.xlsx", index=False, sheet_name="Commits")

        