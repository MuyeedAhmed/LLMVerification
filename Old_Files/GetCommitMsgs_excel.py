import os
import pandas as pd
import subprocess

input_file = 'FFmpeg_CVE_Expanded_List.xlsx'
df = pd.read_excel(input_file)

ffmpeg_repo_path = 'FFmpeg'

messages = []
changed_files = []

for commit in df['Commit Hash']:
    try:
        msg = subprocess.run(
            ['git', 'show', '--no-patch', '--format=%B', commit],
            cwd=ffmpeg_repo_path,
            capture_output=True,
            text=True,
            check=True
        ).stdout.strip()
        messages.append(msg)

        files = subprocess.run(
            ['git', 'show', '--name-only', '--format=', commit],
            cwd=ffmpeg_repo_path,
            capture_output=True,
            text=True,
            check=True
        ).stdout.strip().splitlines()
        changed_files.append(', '.join(files))

    except subprocess.CalledProcessError as e:
        messages.append(f"ERROR: {e.stderr.strip()}")
        changed_files.append("ERROR")

df['Commit Message'] = messages
df['Changed Files'] = changed_files

output_file = 'FFmpeg_CVE_with_Messages_and_Files.xlsx'
df.to_excel(output_file, index=False)
print(f"Updated file saved as: {output_file}")
