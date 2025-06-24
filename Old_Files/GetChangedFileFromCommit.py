import os
import pandas as pd
import shutil
import subprocess
from pathlib import Path


# excel_path = "commit_data_with_clarified_description.xlsx"
# excel_path = "ExcelFiles/Commits_With_Malloc.xlsx"
excel_path = "ExcelFiles/Commits_Jun24-Jun25.xlsx"
parents_csv_path = "ExcelFiles/Commit_Parents.csv"
repo_path = "FFmpeg"
# output_base = "/Users/muyeedahmed/Desktop/Gitcode/Verification/FFmpeg/LLM_Gen"
# output_base = "/Users/muyeedahmed/Desktop/Gitcode/Verification/FFmpeg/LLM_Gen_Malloc"
output_base = "/Users/muyeedahmed/Desktop/Gitcode/Verification/FFmpeg/LLM_Gen_Latest"

df = pd.read_excel(excel_path)
parent_map = pd.read_csv(parents_csv_path, header=None, names=["commit", "parent"])
parent_dict = dict(zip(parent_map["commit"], parent_map["parent"]))


original_branch = subprocess.check_output(
    ["git", "rev-parse", "--abbrev-ref", "HEAD"],
    cwd=repo_path,
    text=True
).strip()

for _, row in df.iterrows():
    commit_hash = str(row["Commit Hash"]).strip()
    # file_names = str(row["file_names"]).strip()
    file_names = str(row["Changed File"]).strip()
    file_list = file_names.split(", ")


    if len(file_list) != 1 or not file_names:
        continue

    rel_path = file_list[0]
    filename = os.path.basename(rel_path)
    name, ext = os.path.splitext(filename)
    dest_dir = os.path.join(output_base, commit_hash)
    os.makedirs(dest_dir, exist_ok=True)

    parent_hash = parent_dict.get(commit_hash)
    if not parent_hash:
        try:
            parent_hash = subprocess.check_output(
                ['git', 'rev-parse', f'{commit_hash}^'],
                cwd=repo_path,
                text=True
            ).strip()
        except subprocess.CalledProcessError:
            print(f"Could not find parent for {commit_hash} using ^ fallback")
            parent_hash = None

    # print(parent_hash)
    if parent_hash:
        subprocess.run(["git", "checkout", parent_hash], cwd=repo_path, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        original_path = os.path.join(repo_path, rel_path)
        if os.path.exists(original_path):
            original_copy = os.path.join(dest_dir, f"{name}_original{ext}")
            shutil.copyfile(original_path, original_copy)
        else:
            print(f"Original file missing at {parent_hash}: {rel_path}")
    else:
        print(f"No parent found for {commit_hash}")

    subprocess.run(["git", "checkout", commit_hash], cwd=repo_path, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    patched_path = os.path.join(repo_path, rel_path)
    if os.path.exists(patched_path):
        patched_copy = os.path.join(dest_dir, f"{name}_human{ext}")
        shutil.copyfile(patched_path, patched_copy)
    else:
        print(f"Patched file missing at {commit_hash}: {rel_path}")
    
    

subprocess.run(["git", "checkout", original_branch], cwd=repo_path)

