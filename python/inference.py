

from transformers import AutoTokenizer
from transformers import T5ForConditionalGeneration


tokenizer = AutoTokenizer.from_pretrained(
    "hyperdb-text2sql/checkpoint-2625", use_fast=True, local_files_only=True
)

model = T5ForConditionalGeneration.from_pretrained(
    "hyperdb-text2sql/checkpoint-2625", local_files_only=True
)


question = "How many heads of the departments are older than 56?"


schema = """
head (head_ID, name, born_state, age)
"""

input_text = f"schema:\n{schema}\nquestion:\n{question}"

inputs = tokenizer(input_text, return_tensors="pt")

outputs = model.generate(**inputs, max_new_tokens=128)

sql = tokenizer.decode(outputs[0], skip_special_tokens=True)

print(sql)
