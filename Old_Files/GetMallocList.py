import pandas as pd


commit_list_txt = 'Commits_With_Malloc.txt'
excel_file = 'FFmpeg_CVE_with_Messages_and_Files.xlsx'
output_file = 'Commits_With_Malloc.xlsx'

with open(commit_list_txt, 'r') as f:
    commit_hashes = {line.strip() for line in f if line.strip()}

df = pd.read_excel(excel_file)

filtered_df = df[df['Commit Hash'].isin(commit_hashes)]

filtered_df.to_excel(output_file, index=False)

