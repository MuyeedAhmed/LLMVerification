import os
import re
import pandas as pd
import csv

FUNC_DEF_RE = re.compile(
    r"""
    (?:[A-Za-z_][\w\s\*\(\),]*?\s+)?
    (?P<name>[A-Za-z_]\w*)\s*
    \([^)]*\)\s*
    \{
    """,
    re.VERBOSE | re.DOTALL,
)


def find_function_range(lines, target_name):
    n = len(lines)
    buffer = ""
    start_idx = 0
    inside_candidate = False

    for i in range(n):
        line = lines[i]
        stripped = line.strip()

        if not stripped or stripped.startswith("#"):
            continue

        buffer += line
        if "{" in line:
            match = FUNC_DEF_RE.search(buffer)
            if match and match.group("name") == target_name:
                start_line = start_idx + 1
                brace_depth = 0
                for j in range(i, n):
                    brace_depth += lines[j].count("{") - lines[j].count("}")
                    if brace_depth == 0:
                        end_line = j + 1
                        return start_line, end_line
                break
            buffer = ""
            start_idx = i + 1
        elif not inside_candidate:
            start_idx = i
            inside_candidate = True

    return None, None


def process_entry(base_dir, commit_hash, function_name):
    commit_dir = os.path.join(base_dir, commit_hash)
    file_line = 0
    if not os.path.isdir(commit_dir):
        print(f"Missing directory: {commit_dir}")
        return None

    for fname in os.listdir(commit_dir):
        if fname.endswith("_llm.c"):
            full_path = os.path.join(commit_dir, fname)
            with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
            llm_start, llm_end = find_function_range(lines, function_name)
        elif fname.endswith("_original.c"):
            full_path = os.path.join(commit_dir, fname)
            with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
            orig_start, orig_end = find_function_range(lines, function_name)
            file_line = len(lines)

    if llm_start and llm_end and orig_start and orig_end:
        return (llm_start, llm_end, orig_start, orig_end, file_line)

    print(f"Function {function_name} not found in {commit_hash}")
    return None

def main():
    excel_path = "ExcelFiles/FFmpeg.xlsx"
    excel_pathOut = "ExcelFiles/FFmpeg_fl.xlsx"
    
    base_dir = "FileHistory/FFmpeg"

    df = pd.read_excel(excel_path)
    results = []

    for _, row in df.iterrows():
        commit = str(row['Commit Hash']).strip()
        func = str(row['Changed Functions']).strip()
        if func == "nan":
            continue
        if len(func.split(",")) > 1:
            print(f"Skipping multi-function entry: {func}")
            continue
        res = process_entry(base_dir, commit, func)
        # print(commit, res)
        if res:
            llm_start, llm_end, orig_start, orig_end, file_line = res
            results.append({
                "Commit Hash": commit,
                "llm_start": llm_start,
                "llm_end": llm_end,
                "orig_start": orig_start,
                "orig_end": orig_end,
                "file_line": file_line
            })

    result_df = pd.DataFrame(results)

    merged_df = df.merge(result_df, on=["Commit Hash"], how="left")

    merged_df.to_excel(excel_pathOut, index=False)



if __name__ == "__main__":
    main()
