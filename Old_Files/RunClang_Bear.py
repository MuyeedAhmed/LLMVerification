import os
import shutil
import subprocess
import pandas as pd

# BASE_DIR = "LLM_Gen_Latest"
BASE_DIR = "LLM_Gen"
FFMPEG_DIR = "FFmpeg"
# EXCEL_PATH = "ExcelFiles/Commits_Jun24-Jun25.xlsx"
EXCEL_PATH = "ExcelFiles/FFmpeg.xlsx"
OUTPUT_DIR = "Clang_Reports"
MISMATCH_FILE = os.path.join(OUTPUT_DIR, "Mismatch.txt")
Summary = os.path.join(OUTPUT_DIR, "Summary.csv")

os.makedirs(OUTPUT_DIR, exist_ok=True)

def run_command(cmd, cwd=None):
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    return result.returncode, result.stdout + result.stderr

def main():
    df = pd.read_excel(EXCEL_PATH)
    df.columns = df.columns.str.strip()

    for folder in os.listdir(BASE_DIR):
        folder_path = os.path.join(BASE_DIR, folder)
        if not os.path.isdir(folder_path):
            continue

        llm_file = next((f for f in os.listdir(folder_path) if f.endswith('_llm.c')), None)
        if not llm_file:
            continue

        commit_hash = folder

        orig_out_path = os.path.join(OUTPUT_DIR, f"{commit_hash}_original.txt")
        llm_out_path = os.path.join(OUTPUT_DIR, f"{commit_hash}_llm.txt")
        if os.path.exists(orig_out_path) and os.path.exists(llm_out_path):
            print(f"Skipping {commit_hash}, output files already exist.")
            continue

        row = df[df['Commit Hash'] == commit_hash]
        if row.empty:
            print(f"No entry in Excel for {commit_hash}")
            continue
        
        print(f"\n=== Checking out {commit_hash} ===")
        code, out = run_command(f"git checkout {commit_hash}", cwd=FFMPEG_DIR)
        print(out)
        if code != 0:
            continue

        changed_file = row.iloc[0]['File Names']
        commit_message = row.iloc[0]['Commit Description']
        changed_file_path = os.path.join(FFMPEG_DIR, changed_file)
        if not os.path.exists(changed_file_path):
            print(f"Changed file missing: {changed_file_path}")
            continue

        

        # print("=== Running configure ===")
        # code, out = run_command("./configure && make config", cwd=FFMPEG_DIR)
        # if code != 0:
        #     code, out = run_command("./configure --disable-asm", cwd=FFMPEG_DIR)
        #     print(out)
        #     if code != 0:
        #         continue

        rel_path = os.path.relpath(changed_file_path, FFMPEG_DIR)
        backup_path = changed_file_path + ".bak"
        llm_src_path = os.path.join(folder_path, llm_file)

        print(f"=== Analyzing original {rel_path} ===")

        code, clang_output = run_analyzer_script(rel_path, FFMPEG_DIR, orig_out_path)


        print("=== Replacing with LLM version and reanalyzing ===")
        shutil.copy2(changed_file_path, backup_path)
        shutil.copy2(llm_src_path, changed_file_path)
        
        code, clang_output_llm = run_analyzer_script(rel_path, FFMPEG_DIR, llm_out_path)


        shutil.move(backup_path, changed_file_path)

        if clang_output != clang_output_llm:
            with open(MISMATCH_FILE, "a") as f:
                f.write(f"=== Commit: {commit_hash} ===\n")
                f.write("--- Original Output ---\n")
                f.write(clang_output + "\n")
                f.write("--- LLM Output ---\n")
                f.write(clang_output_llm + "\n\n")
            with open(Summary, "a") as summary_file:
                # summary_file.write(f"{commit_hash},{commit_message},{changed_file},Differences found\n")
                summary_file.write(f"{commit_hash},{changed_file},Differences found\n")
        else:
            with open(Summary, "a") as summary_file:
                # summary_file.write(f"{commit_hash},{commit_message},{changed_file},No differences\n")
                summary_file.write(f"{commit_hash},{changed_file},No differences\n")

def run_analyzer_script(target_path, cwd, output_path):
    script_cmd = f"./analyze.sh {target_path}"
    code, clang_output = run_command(script_cmd, cwd=cwd)
    with open(output_path, "w") as f:
        f.write(clang_output)
    print(f"Saved: {output_path}")
    return code, clang_output

if __name__ == "__main__":
    main()
