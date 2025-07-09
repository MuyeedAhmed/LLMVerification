import pandas as pd
from scipy.stats import ttest_ind

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

    t_stat, p_val = ttest_ind(col1_clean, col2_clean, equal_var=False)

    print(f"Mean of {c1}: {mean1}")
    print(f"Mean of {c2}: {mean2}")
    print(f"Two-tailed p-value: {p_val}")


total_func_c1 = 'RSM Output_Total Functions'
total_func_c2 = 'ChatGPT Output_Total Functions'

run_t_test(total_func_c1, total_func_c2)

total_loc_c1 = 'RSM Output_Total eLOC'
total_loc_c2 = 'ChatGPT Output_Total eLOC'

run_t_test(total_loc_c1, total_loc_c2)
