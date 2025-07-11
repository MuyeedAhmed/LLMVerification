import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import numpy as np
from scipy.stats import ttest_rel, pearsonr

df = pd.read_csv('Complexity.csv')  



def plot_sorted_data(df):
    df_sorted = df.sort_values(by=df.columns[1]).reset_index(drop=True)
    A = df_sorted.iloc[:, 0]
    B = df_sorted.iloc[:, 1]
    C = df_sorted.iloc[:, 2]
    # A = df_sorted.iloc[:75, 0]
    # B = df_sorted.iloc[:75, 1]
    # C = df_sorted.iloc[:75, 2]

    plt.figure(figsize=(12, 5))
    plt.subplot(1, 2, 1)
    plt.plot(B, label=df.columns[1])
    plt.plot(A, label=df.columns[0])
    plt.xlabel("")
    plt.ylabel("Count")
    plt.legend()
    plt.grid(True)
    plt.subplot(1, 2, 2)
    plt.plot(B, label=df.columns[1])
    plt.plot(C, label=df.columns[2])
    plt.xlabel("")
    plt.ylabel("Count")
    plt.legend()
    plt.grid(True)

    plt.tight_layout()
    plt.show()

A = df.iloc[:, 0]
B = df.iloc[:, 1]
C = df.iloc[:, 2]
# A = df.iloc[:75, 0]
# B = df.iloc[:75, 1]
# C = df.iloc[:75, 2]

def compare(x, y, label1, label2):
    mae = np.mean(np.abs(x - y))
    mse = np.mean((x - y)**2)
    corr, _ = pearsonr(x, y)
    p_val = ttest_rel(x, y).pvalue
    
    print(f"== {label1} vs {label2} ==")
    print(f"MAE:  {mae:.4f}")
    print(f"MSE:  {mse:.4f}")
    print(f"Pearson Corr: {corr:.4f}")
    print(f"Paired t-test p-value: {p_val:.4g}")
    print()


# Run comparisons
compare(A, B, "SCC", "MCCabe")
compare(A, C, "SCC", "LLM")
compare(B, C, "MCCabe", "LLM")



from sklearn.linear_model import LinearRegression

reg = LinearRegression().fit(B.values.reshape(-1,1), A)
residuals = A - reg.predict(B.values.reshape(-1,1))
mse_residual = np.mean(residuals ** 2)
print(f"Mean Squared Error of Residuals (SCC, MCCabe): {mse_residual:.4f}")

reg_BC = LinearRegression().fit(B.values.reshape(-1,1), C)
residuals_BC = C - reg_BC.predict(B.values.reshape(-1,1))
mse_residual_BC = np.mean(residuals_BC ** 2)
print(f"Mean Squared Error of Residuals (MCCabe, LLM): {mse_residual_BC:.4f}")


plot_sorted_data(df)