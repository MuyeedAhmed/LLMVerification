import pandas as pd
from scipy.stats import ttest_ind
from scipy import stats
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

df_raw = pd.read_excel("Code Metrics Data.xlsx", header=[0, 1])

df_raw.columns = [f"{tool}_{field}" for tool, field in df_raw.columns]


# print(df_raw.columns)

def run_t_test(c1, c2):
    col1 = df_raw[c1]
    col2 = df_raw[c2]

    valid = col1.notna() & col2.notna()
    col1_clean = col1[valid]
    col2_clean = col2[valid]

    mean1 = col1_clean.mean()
    mean2 = col2_clean.mean()

    t_stat, p_val = stats.ttest_rel(col1_clean, col2_clean)

    print(f"Mean of {c1}: {mean1}")
    print(f"Mean of {c2}: {mean2}")
    print(f"Two-tailed p-value: {p_val}")

def thousands_formatter(x, pos):
    return f'{int(x/1000)}k'

def Plot(c1, c2, stat):
    sorted_df = df_raw.sort_values(by=c1).reset_index(drop=True)
    col1 = sorted_df[c1]
    col2 = sorted_df[c2]

    valid = col1.notna() & col2.notna()
    col1_clean = col1[valid]
    col2_clean = col2[valid]

    x = np.arange(len(col1_clean))
    width = 0.35

    fig, ax = plt.subplots(figsize=(6, 3))
    # bar1 = ax.bar(x - width/2, col1_clean, width, label=c1)
    # bar2 = ax.bar(x + width/2, col2_clean, width, label=c2)
    plt.scatter(x, col1_clean, label="Actual", marker='o', s=5, color='orange')
    plt.scatter(x, col2_clean, label="ChatGPT", marker='x', s=15, color='blue')
    # ax.set_ylabel(stat)
    ax.set_xticks([])
    if stat == 'Total eLOC':
        ax.yaxis.set_major_formatter(FuncFormatter(thousands_formatter))
    ax.legend()

    plt.tight_layout()
    plt.savefig(f"Figures/{stat}.pdf", dpi=300, bbox_inches='tight')

''' Total Functions '''
total_func_c1 = 'RSM Output_Total Functions'
total_func_c2 = 'ChatGPT Output_Total Functions'

run_t_test(total_func_c1, total_func_c2)
Plot(total_func_c1, total_func_c2, 'Total Function Count')

""" Total Lines of Code """
total_loc_c1 = 'RSM Output_Total eLOC'
total_loc_c2 = 'ChatGPT Output_Total eLOC'

run_t_test(total_loc_c1, total_loc_c2)
Plot(total_loc_c1, total_loc_c2, 'Total eLOC')

""" Return Points """
total_rp_c1 = 'RSM Output_Avg Return Points'
total_rp_c2 = 'ChatGPT Output_Avg Return Points'

run_t_test(total_rp_c1, total_rp_c2)
Plot(total_rp_c1, total_rp_c2, 'Average Return Points')

""" Average Parameters """

total_ap_c1 = 'RSM Output_Avg Parameters'
total_ap_c2 = 'ChatGPT Output_Avg Parameters'

run_t_test(total_ap_c1, total_ap_c2)
Plot(total_ap_c1, total_ap_c2, 'Average Parameters')