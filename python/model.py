#import sqlite
import sqlite3

#so python can install datasets from hugging face
from datasets import load_dataset

#import transformers library for tokenizer
import transformers

#import tokenizer
from transformers import T5Tokenizer
#import the T5 model class
from transformers import T5ForConditionalGeneration
#for padding and bathcing
from transformers import DataCollatorForSeq2Seq
#import the trainer to run the training loops
from transformers import Trainer


#to tell hugging face how we want the model trained
from transformers import TrainingArguments

#load the tokenizer for the t5 small model
tokenizer = T5Tokenizer.from_pretrained("t5-small")

#load the T5 small model into model variable
model = T5ForConditionalGeneration.from_pretrained("t5-small")

#load dataser into ds
ds = load_dataset("xlangai/spider")

#look at the train dataset
train_data = ds["train"]



#take a training example and convert it to a frmat for our model
def format_example(example):
    #get the id in the example - tells us which database it belongs to
    db_id = example["db_id"]
    #get the question from the example
    question = example["question"]
    #get the sql query it generated
    query = example["query"]
    #now construct the appropriate db_path

    db_path = f"data/text2sql/spider_data/database/{db_id}/{db_id}.sqlite"
    #open a connection to the department management database from spider
    connection = sqlite3.connect(db_path )

    #create a cursor to send sql commands to the database
    cursor = connection.cursor()
    # Ask for the names of all the tables in the database
    cursor.execute("""
    SELECT name
    FROM sqlite_master
    WHERE type = 'table';
    """)

    # Get those names and store them in tables
    tables = cursor.fetchall()

    # String to store the schema to send to the model
    schema = ""

    # Loop through each table
    for (table_name,) in tables:
        # Get information about the table's columns
        cursor.execute(f'PRAGMA table_info("{table_name}")')

        # Store the column information
        columns = cursor.fetchall()

        # Get just the column names
        column_names = [column[1] for column in columns]

        # Add this table and its columns to the schema
        schema += f"{table_name} ({', '.join(column_names)})\n"

    # Combine the schema and question into the model's input
    input_text = f"schema:\n{schema}\nquestion:\n{question}"

    # Close the database connection
    connection.close()

    # Return the model input and correct SQL from the dataset not the sql we generated
    return {
        "input": input_text,
        "target": query
    }
    


#take the output from fromat example and generate tokens that the model understands
def tokenize_example(example):
    #convert the input part of the formatted ecampel into t5 token ids
    inputs = tokenizer(example["input"], truncation=True)
    #do the same for the generated SQL
    labels = tokenizer(text_target=example["target"], truncation=True)
    #attach the token id mapping for the query (ie. labels) to the inputs
    inputs["labels"] = labels["input_ids"]
    #this whay u can just return the inputs dictionary
    return inputs

#format data
#apply the format_example function accross the hwole train dataser
train_data = train_data.map(format_example)

#tokenieze data
#apply tokenizer to traindata

train_data = train_data.map(tokenize_example)


#now for the validaton data
validation_data = ds["validation"]

validation_data = validation_data.map(format_example)

validation_data = validation_data.map(tokenize_example)


#create data collator
data_collator = DataCollatorForSeq2Seq(
    tokenizer=tokenizer,
    model=model
)

#here are the training arguments
training_args = TrainingArguments(
    output_dir="./hyperdb-text2sql",
    per_device_train_batch_size=8,
    eval_strategy="epoch",
    num_train_epochs=3,
    logging_steps=100,
    save_steps=500,
)

#configure the trainer
trainer = Trainer(
    model=model,
    args=training_args,
    train_dataset=train_data,
    eval_dataset = validation_data,
    data_collator=data_collator,
)

#train!!!!
trainer.train()
