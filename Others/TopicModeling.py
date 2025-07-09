import nltk
from nltk.corpus import stopwords
from nltk.tokenize import word_tokenize
from gensim import corpora, models
import pandas as pd
nltk.download('punkt')
nltk.download('stopwords')

# file1_path = 'TopicModel/GNU.xlsx'
# file2_path = 'TopicModel/FFmpeg.xlsx'

# df1 = pd.read_excel(file1_path, sheet_name='GitHub - Copilot(updated)')
# df2 = pd.read_excel(file2_path)

# desc_list = df1["Function Description"].dropna().astype(str).tolist()
# commit_list = df2["Commit message"].dropna().astype(str).tolist()

# documents = desc_list + commit_list

documents = pd.read_excel('TopicModel/All.xlsx')['Commit Description'].dropna().astype(str).tolist()
print(len(documents), "documents loaded for topic modeling.")
stop_words = set(stopwords.words('english'))
processed_docs = [
    [word.lower() for word in word_tokenize(doc) if word.isalpha() and word.lower() not in stop_words]
    for doc in documents
]

dictionary = corpora.Dictionary(processed_docs)
corpus = [dictionary.doc2bow(doc) for doc in processed_docs]

num_topics = 5
lda_model = models.LdaModel(corpus, num_topics=num_topics, id2word=dictionary, passes=15)

for idx, topic in lda_model.print_topics(num_words=5):
    print(f"Topic #{idx + 1}: {topic}")
