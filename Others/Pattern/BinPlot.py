import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy import stats

df = pd.read_excel("All.xlsx")

df["Result"] = df["Manual Inspection (LLM)"].map({"Correct": 1, "Wrong": 0, np.nan: 0})


def plot_bin_counts(column_name):
    bin_count = 3 if column_name == "Diff Size" else 8

    df_bin = df[[column_name, "Result", "LLM"]].dropna()
    df_bin["bin"] = pd.qcut(df_bin[column_name], q=bin_count, duplicates="drop")

    grouped = df_bin.groupby(["bin", "LLM"])
    stats = grouped["Result"].agg(total="count", correct="sum")
    stats["correct_pct"] = stats["correct"] / stats["total"] * 100
    stats = stats.reset_index()

    bins = stats["bin"].astype(str).unique()
    llms = stats["LLM"].unique()

    x = np.arange(len(bins))
    width = 0.8 / len(llms)

    plt.figure(figsize=(5, 3))

    for i, llm in enumerate(llms):
        llm_data = stats[stats["LLM"] == llm]
        y = llm_data["correct_pct"].values
        plt.bar(x + i * width, y, width, label=llm)

    plt.xlabel(column_name)
    plt.ylabel("Success Rate (%)")

    if column_name == "Context File Size":
        labels = ["[32-276]", "[277-479]", "[480-602]", "[603-774]",
                  "[745-1066]", "[1067-2335]", "[2336-7695]", "[7696-125527]"]
    elif column_name == "Function Size":
        labels = ["[5-14]", "[15-25]", "[26-43]", "[44-55]",
                  "[56-90]", "[91-131]", "[132-191]", "[192-816]"]
    elif column_name == "Diff Size":
        labels = ["[1-3]", "[3-8]", "[9-66]"]
    else:
        labels = bins

    plt.xticks(x + width * (len(llms) - 1) / 2, labels, rotation=45, ha="center")

    plt.legend()
    plt.tight_layout()
    plt.savefig(f"Figures/{column_name}_by_LLM.pdf")

def ttests(column_name):
    df_c = df[[column_name, "Result", "LLM"]].dropna()
    llms = df_c["LLM"].unique()
    for llm in llms:
        llm_data = df_c[df_c["LLM"] == llm]
        success_group = llm_data[llm_data['Result'] == 1][column_name]
        failure_group = llm_data[llm_data['Result'] == 0][column_name]

        # Welch's T-Test (does not assume equal variance or sample size)
        t_stat, p_val = stats.ttest_ind(success_group, failure_group, equal_var=False)

        print(f"--- Welch's T-Test on Raw Data {column_name} - {llm} ---")
        print(f"Mean size (Success): {success_group.mean():.2f}")
        print(f"Mean size (Failure): {failure_group.mean():.2f}")
        print(f"T-statistic: {t_stat:.4f}")
        print(f"P-value: {p_val:.4f}")

        if p_val < 0.05:
            print("Result: Statistically Significant. File size differs between outcomes.")
        else:
            print("Result: Not Significant. File size is not a primary driver of outcome.")

for col in ["Context File Size", "Function Size", "Diff Size"]:
    plot_bin_counts(col)
    ttests(col)
