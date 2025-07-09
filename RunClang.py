import os
import shutil
import subprocess
import pandas as pd
from ParseClangReport import parse_clang_report, save_to_excel

def run_command(cmd, cwd=None):
    result = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    return result.returncode, result.stdout + result.stderr

def run_analyzer_script(target_path, cwd, output_path):
    script_cmd = f"./analyze.sh {target_path}"
    code, clang_output = run_command(script_cmd, cwd=cwd)
    with open(output_path, "w") as f:
        f.write(clang_output)
    print(f"Saved: {output_path}")
    return code, clang_output

def CheckIfConfigExists(project):
    config_path = f"Projects/{project}/.git/config"
    if not os.path.exists(config_path):
        print(f"Config file not found for project {project}.")
        return False
    return True

def CopyAnalyzerScript(project):
    script_path = "analyze.sh"
    project_dir = os.path.join("Projects", project)
    if os.path.exists(os.path.join(project_dir, script_path)):
        return
    shutil.copy2(script_path, project_dir)
    run_command(f"chmod +x {script_path}", cwd=project_dir)
    
def RunClang(project, commit_hash, change_file):
    CopyAnalyzerScript(project)
    EXCEL_PATH = f"ExcelFiles/{project}_{commit_hash}.xlsx"
    BASE_DIR = f"FileHistory/{project}"
    PROJ_DIR = os.path.join("Projects", project)
    OUTPUT_DIR = os.path.join("Clang_Reports", project)
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    change_file_name = change_file.split("/")[-1].split(".")[0]
    llm_src_path = os.path.join(BASE_DIR, commit_hash, change_file_name + "_llm.c")
    original_file_dir = os.path.join(BASE_DIR, commit_hash, change_file_name + "_original.c")

    orig_out_path = os.path.join(OUTPUT_DIR, f"{commit_hash}_original.txt")
    llm_out_path = os.path.join(OUTPUT_DIR, f"{commit_hash}_llm.txt")
    if os.path.exists(orig_out_path) and os.path.exists(llm_out_path):
        print(f"Skipping {commit_hash}, output files already exist.")
        return
        
    if not os.path.exists(llm_src_path):
        print(f"LLM file not found: {llm_src_path}")
        return
    if not os.path.exists(original_file_dir):
        print(f"Original file not found: {original_file_dir}")
        return

    print(f"\n=== Checking out {commit_hash} ===")
    code, out = run_command(f"git checkout {commit_hash}", cwd=PROJ_DIR)
    # print(out)
    if code != 0:
        return

    changed_file_path = os.path.join(PROJ_DIR, change_file)
    if not os.path.exists(changed_file_path):
        print(f"Changed file missing: {changed_file_path}")
        return

    rel_path = os.path.relpath(changed_file_path, PROJ_DIR)

    backup_path = changed_file_path + ".bak"

    print(f"=== Analyzing original {rel_path} ===")

    code, clang_output = run_analyzer_script(rel_path, PROJ_DIR, orig_out_path)

    print("=== Replacing with LLM version and reanalyzing ===")
    shutil.copy2(changed_file_path, backup_path)
    shutil.copy2(llm_src_path, changed_file_path)
    
    code, clang_output_llm = run_analyzer_script(rel_path, PROJ_DIR, llm_out_path)

    shutil.move(backup_path, changed_file_path)

    # Parse the clang output and save to Excel
    issues = parse_clang_report(orig_out_path, code_by="Original")
    issues_llm = parse_clang_report(llm_out_path, code_by="LLM")
    all_issues = issues + issues_llm
    if all_issues:
        save_to_excel(all_issues, output_file=EXCEL_PATH)
    else:
        print(f"No issues found in the report for {commit_hash}.")


