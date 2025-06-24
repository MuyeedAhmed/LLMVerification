from ExtractGitInfo import ExtractGitInfo
from openpyxl import Workbook
import os

def main():
    github_link = "https://github.com/FFmpeg/FFmpeg.git"
    repo_path = "FFmpeg"
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
        changed_function = run_clang.GetChangedFunctions(commit)
        prompt = f"Fix Requirement: {message}\nFunction:{changed_function}\nGive me the edited function. Also show the diff."
        
        if len(changed_files) == 1:
            filtered_commits.append((commit, commit_date, message, prompt, changed_files[0], changed_function))
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
    for commit, commit_date, message, prompt, file, changed_function in filtered_commits:
        ws.append([commit, commit_date, message, prompt, file, changed_function])
    wb.save("Test.xlsx")


if __name__ == "__main__":
    main()
