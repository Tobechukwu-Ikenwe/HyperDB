# Load the Spider dataset
from datasets import load_dataset

ds = load_dataset("xlangai/spider")

# Show the first training example
print(ds["train"][0])

# Show the dataset's available features
print(ds["train"].features)
