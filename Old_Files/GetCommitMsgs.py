import os
import pandas as pd
import subprocess

excel_file = 'FFmpeg_CVE_Expanded_List.xlsx'

ffmpeg_repo_path = 'FFmpeg'

msg_output_dir = 'CVE_Commit_Messages'
diff_output_dir = 'CVE_Commit_Diffs'
os.makedirs(msg_output_dir, exist_ok=True)
os.makedirs(diff_output_dir, exist_ok=True)

df = pd.read_excel(excel_file)
commit_hashes = df['Commit Hash'].dropna().unique()

for commit in commit_hashes:
    try:
        msg_result = subprocess.run(
            ['git', 'show', '--no-patch', '--format=%B', commit],
            cwd=ffmpeg_repo_path,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        with open(os.path.join(msg_output_dir, f'{commit}.txt'), 'w', encoding='utf-8') as f:
            f.write(msg_result.stdout.strip())
        print(f"Saved message for {commit}")

        diff_result = subprocess.run(
            ['git', 'show', '--format=', commit],
            cwd=ffmpeg_repo_path,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        with open(os.path.join(diff_output_dir, f'{commit}.diff'), 'w', encoding='utf-8') as f:
            f.write(diff_result.stdout)
        print(f"Saved diff for {commit}")

    except subprocess.CalledProcessError as e:
        print(f"Error with commit {commit}: {e.stderr.strip()}")
