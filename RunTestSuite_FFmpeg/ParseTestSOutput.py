import pandas as pd
import os
import re
import numpy as np


def ReadReportFile(file_path):
    with open(file_path, 'r') as file:
        content = file.read()
    return content

def ParseTestOutput(output, target_file):
    lines = output.splitlines()
    results = []

    i = 0
    while i < len(lines):
        if target_file in lines[i]:
            results.append(lines[i].strip())
            if i + 1 < len(lines):
                results.append(lines[i + 1].strip())
            if i + 2 < len(lines):
                results.append(lines[i + 2].strip())
        i += 1

    return results

def main():
    EXCEL_PATH = "ExcelFiles/FFmpeg.xlsx"
    REPORT_PATH = "Test_Reports"
    REPORT_SUMMARY_PATH = "Test_Reports/Summary"
    os.makedirs(REPORT_SUMMARY_PATH, exist_ok=True)

    df = pd.read_excel(EXCEL_PATH)
    commit_hashes = df['Commit Hash'].tolist()

    for commit_hash in commit_hashes:
        target_file = df.loc[df['Commit Hash'] == commit_hash, 'File Names'].values[0]
        report_file_llm = os.path.join(REPORT_PATH, f"{commit_hash}_llm.txt")
        report_file_orig = os.path.join(REPORT_PATH, f"{commit_hash}_original.txt")
        report_summary_file = os.path.join(REPORT_SUMMARY_PATH, f"{commit_hash}.txt")

        if not os.path.exists(report_file_llm):
            continue
        
        output_orig = ReadReportFile(report_file_orig)
        results_orig = ParseTestOutput(output_orig, target_file)
        output_llm = ReadReportFile(report_file_llm)
        results_llm = ParseTestOutput(output_llm, target_file)
        if len(results_orig)==0 and len(results_llm)==0:
            continue

        with open(report_summary_file, 'w') as summary_file:
            summary_file.write("====ORIGINAL====\n")
            summary_file.writelines("\n".join(results_orig))
            summary_file.write("\n====LLM====\n")
            summary_file.writelines("\n".join(results_llm))

main()