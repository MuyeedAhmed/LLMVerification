import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_excel("All.xlsx")

df["Result"] = df["Manual Inspection (LLM)"].map({"Correct": 1, "Wrong": 0})


def plot_bin_counts(column_name):
    bin_count = 3 if column_name == "Diff Size" else 8

    df_bin = df[[column_name, "Result"]].dropna()
    df_bin['bin'] = pd.qcut(df_bin[column_name], q=bin_count, duplicates='drop')
    bin_edges = df_bin['bin'].cat.categories
    bin_centers = [(b.left + b.right) / 2 for b in bin_edges]
    
    grouped = df_bin.groupby('bin')
    total_counts = grouped['Result'].count()
    correct_counts = grouped['Result'].sum()
    correct_pct = correct_counts / total_counts * 100

    plt.figure(figsize=(5, 3))
    bins = total_counts.index.astype(str)

    # plt.bar(bins, total_counts, label='Total', color='lightgray')
    plt.bar(bins, correct_pct)

    # for i, (cc, pct) in enumerate(zip(correct_counts, correct_pct)):
    #     if cc > 0:
    #         plt.text(i, cc / 2, f'{pct:.0f}%', ha='center', va='center', fontsize=10, color='white')

    plt.xlabel(f'{column_name}')
    plt.ylabel('Success Rate (%)')
    
    # plt.xticks(ticks=range(len(bin_centers)), labels=[f'{x:.1f}' for x in bin_centers])
    
    plt.xticks(rotation=45)
    if column_name == "Context File Size":
        plt.xticks(ticks=range(8), labels=["[32-262]", "[263-457]", "[458-565]", "[566-702]", "[703-947]", "[948-1792]", "[1793-4448]", "[4449-125527]"], rotation=45, ha='center')
    elif column_name == "Function Size":
        plt.xticks(ticks=range(8), labels=["[5-13]", "[14-21]", "[22-35]", "[36-49]", "[50-66]", "[67-109]", "[109-174]", "[175-816]"], rotation=45, ha='center')
    elif column_name == "Diff Size":
        plt.xticks(ticks=range(3), labels=["[1-3]", "[3-8]", "[9-66]"])
    else:
        plt.xticks(rotation=45)


    plt.tight_layout()
    plt.savefig(f"Figures/{column_name}.pdf")


for col in ['Context File Size', 'Function Size', 'Diff Size']:
    plot_bin_counts(col)
