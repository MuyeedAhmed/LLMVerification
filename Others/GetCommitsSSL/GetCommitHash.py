import requests

REPO_OWNER = "wolfSSL"
REPO_NAME = "wolfssl"
PR_NUMBERS = [
    8268, 8294, 8275, 8202, 8209, 8226, 7840, 7738, 7761, 7710, 7742,
    7707, 8038, 7389, 7388, 7085, 7299, 7190, 6942, 6947, 7001, 6998
]

def get_commits(pr_number):
    url = f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/pulls/{pr_number}/commits"
    response = requests.get(url)

    if response.status_code != 200:
        print(f"[PR #{pr_number}] Failed: {response.status_code} {response.text}")
        return []

    return [(c['sha'], c['commit']['message'].splitlines()[0]) for c in response.json()]

def main():
    for pr in PR_NUMBERS:
        print(f"\n--- Commits for PR #{pr} ---")
        for sha, msg in get_commits(pr):
            print(f"{sha[:7]}: {msg}")

if __name__ == "__main__":
    main()
