import pandas as pd
import subprocess
import os

def get_loc(commit, file_path):
    try:
        output = subprocess.check_output(
            ['git', 'show', f'{commit}:{file_path}'],
            stderr=subprocess.DEVNULL
        )
        return len(output.decode('utf-8').splitlines())
    except subprocess.CalledProcessError:
        return None

def main(input_excel, output_excel):
    df = pd.read_excel(input_excel)

    locs = []
    for i, row in df.iterrows():
        loc = get_loc(row['Commit Hash'], row['Changed File'])
        locs.append(loc)

    df['loc'] = locs
    df.to_excel(output_excel, index=False)


if __name__ == "__main__":
    excel_input = "FFmpeg.xlsx"
    excel_output = "FFmpeg_loc.xlsx"
    main(excel_input, excel_output)
