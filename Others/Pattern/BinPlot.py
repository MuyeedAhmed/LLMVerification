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

    pivot = df_bin.pivot_table(index="bin", columns="LLM", values="Result", 
                               aggfunc=["sum", "count"], observed=False).fillna(0)
    
    bins = pivot.index
    llms = pivot["sum"].columns
    x = np.arange(len(bins))
    width = 0.8 / len(llms)

    plt.figure(figsize=(5, 3))
    all_x_centers = x + width * (len(llms) - 1) / 2

    for i, llm in enumerate(llms):
        y = (pivot["sum"][llm] / pivot["count"][llm] * 100).fillna(0).values
        x_llm = x + i * width
        bars = plt.bar(x_llm, y, width, label=llm)
        
        ### Curve Fit
        color = bars[0].get_facecolor()
        if len(y) > 1:
            degree = min(3, len(y) - 1)
            z = np.polyfit(x_llm, y, degree)
            p = np.poly1d(z)
            x_smooth = np.linspace(x_llm.min(), x_llm.max(), 100)
            plt.plot(x_smooth, p(x_smooth), color=color, linestyle='--',linewidth=1, alpha=0.6)
    
    ### Combined curve fit
    # combined = df_bin.groupby("bin", observed=False)["Result"].agg(["sum", "count"]).fillna(0)
    # y_combined = (combined["sum"] / combined["count"] * 100).fillna(0).values
    
    # if len(y_combined) > 1:
    #     degree = min(3, len(y_combined) - 1)
    #     z = np.polyfit(all_x_centers, y_combined, degree)
    #     p = np.poly1d(z)
    #     x_smooth = np.linspace(all_x_centers.min(), all_x_centers.max(), 100)
    #     plt.plot(x_smooth, p(x_smooth), color='black', linestyle='--', linewidth=2, label='Combined Fit',alpha=0.6)

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
        labels = [str(b) for b in bins]

    if len(labels) != len(bins):
        labels = [str(b) for b in bins]

    plt.xticks(all_x_centers, labels, rotation=45, ha="center")

    plt.legend()
    plt.tight_layout()
    plt.savefig(f"Figures/{column_name}_by_LLM.pdf", format='pdf', bbox_inches='tight')

def ttests(column_name):
    df_c = df[[column_name, "Result", "LLM"]].dropna()
    llms = df_c["LLM"].unique()
    for llm in llms:
        llm_data = df_c[df_c["LLM"] == llm]
        success_group = llm_data[llm_data['Result'] == 1][column_name]
        failure_group = llm_data[llm_data['Result'] == 0][column_name]

        t_stat, p_val = stats.ttest_ind(success_group, failure_group, equal_var=False)

        print(f"--- Welch's T-Test on Raw Data {column_name} - {llm} ---")
        print(f"Mean size (Success): {success_group.mean():.2f}")
        print(f"Mean size (Failure): {failure_group.mean():.2f}")
        print(f"T-statistic: {t_stat:.4f}")
        print(f"P-value: {p_val:.4f}")

for col in ["Context File Size", "Function Size", "Diff Size"]:
    plot_bin_counts(col)
    ttests(col)
