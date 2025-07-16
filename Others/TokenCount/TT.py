import tiktoken

def count(filename):
    with open(filename, "r", encoding="utf-8") as f:
        text = f.read()

    # Use the tokenizer that matches GPT-4-turbo
    enc = tiktoken.get_encoding("cl100k_base")
    tokens = enc.encode(text)
    return len(tokens)

# filename = "dpx.c"
# print(filename, count(filename))

# filename = "aacdec.c"
# print(filename, count(filename))
# filename = "avltree.c"
# print(filename, count(filename))
# filename = "atrac3plus.c"
# print(filename, count(filename))
# filename = "cc_hashtable.c"
# print(filename, count(filename))


enc = tiktoken.get_encoding("cl100k_base")
tokens = enc.encode("printf(\"hello\\n\");")

print(tokens)
print(len(tokens))
