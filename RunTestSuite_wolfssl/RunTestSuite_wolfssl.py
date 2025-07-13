import os
import shutil
import subprocess
import pandas as pd

BASE_DIR = "../FileHistory/wolfssl"
PROJ_DIR = "../Projects/wolfssl"
EXCEL_PATH = "../ExcelFiles/wolfssl.xlsx"
OUTPUT_DIR = "Test_Reports"

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
        code, out = run_command(f"git checkout {commit_hash}", cwd=PROJ_DIR)
        print(out)
        if code != 0:
            continue

        changed_file = row.iloc[0]['Changed File']
        commit_message = row.iloc[0]['Commit Message']
        changed_file_path = os.path.join(PROJ_DIR, changed_file)
        if not os.path.exists(changed_file_path):
            print(f"Changed file missing: {changed_file_path}")
            continue

        rel_path = os.path.relpath(changed_file_path, PROJ_DIR)
        
        print(f"=== Analyzing original {rel_path} ===")
        code, test_output = run_analyzer_script(rel_path, PROJ_DIR, orig_out_path)

        print("=== Replacing with LLM version and reanalyzing ===")
        backup_path = changed_file_path + ".bak"
        llm_src_path = os.path.join(folder_path, llm_file)
        
        shutil.copy2(changed_file_path, backup_path)
        shutil.copy2(llm_src_path, changed_file_path)
        
        code, clang_output_llm = run_analyzer_script(rel_path, PROJ_DIR, llm_out_path)


        shutil.move(backup_path, changed_file_path)

def run_analyzer_script(target_path, cwd, output_path):
    script_cmd = f"./RunTestS.sh"
    code, test_output = run_command("./RunTestS.sh", cwd=cwd)
    with open(output_path, "w") as f:
        f.write(test_output)
    return code, test_output

if __name__ == "__main__":
    main()
