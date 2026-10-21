import streamlit as st
import sqlite3
import pandas as pd
from pathlib import Path

from transformers import AutoTokenizer, T5ForConditionalGeneration
from hyperdb_bridge import execute_sqlite_query


# Load our trained Text-to-SQL model
ROOT = Path(__file__).resolve().parents[1]
MODEL_PATH = ROOT / "hyperdb-text2sql/checkpoint-2625"

@st.cache_resource
def load_model():
    tokenizer = AutoTokenizer.from_pretrained(str(MODEL_PATH), use_fast=True,
                                              local_files_only=True)
    model = T5ForConditionalGeneration.from_pretrained(str(MODEL_PATH),
                                                       local_files_only=True).to("cpu")
    return tokenizer, model

tokenizer, model = load_model()

# Build the interface
st.title("HyperDB")
st.caption("AI-Native Database Engine")


# Let the user choose a database
database = st.selectbox(
    "Select a database",
    ["department_management"]
)
engine = st.selectbox("Execute generated SQL with", ["SQLite", "HyperDB C++ subset"])


# Find the selected SQLite database
db_path = ROOT / "data/text2sql/spider_data/database" / database / f"{database}.sqlite"

connection = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
connection.execute("PRAGMA query_only = ON")
cursor = connection.cursor()


# Find all tables in the database
cursor.execute("""
    SELECT name
    FROM sqlite_master
    WHERE type = 'table';
""")

tables = cursor.fetchall()

schema = ""


# Extract the columns from every table
for (table_name,) in tables:
    quoted = table_name.replace('"', '""')
    cursor.execute(f'PRAGMA table_info("{quoted}")')
    columns = cursor.fetchall()

    column_names = [column[1] for column in columns]

    schema += f"{table_name} ({', '.join(column_names)})\n"


# Let the user ask a question
question = st.text_input("Ask your database a question")


# Generate SQL from the question
if st.button("Generate SQL"):
    if not question.strip():
        st.warning("Enter a question before generating SQL.")
    else:
        input_text = f"schema:\n{schema}\nquestion:\n{question}"
        inputs = tokenizer(input_text, return_tensors="pt")
        inputs = {key: value.to("cpu") for key, value in inputs.items()}
        outputs = model.generate(**inputs, max_new_tokens=128)
        st.session_state["generated_sql"] = tokenizer.decode(
            outputs[0], skip_special_tokens=True
        )

if "generated_sql" in st.session_state:
    st.subheader("Review SQL")
    sql = st.text_area("Review or edit SQL", key="generated_sql")
    if st.button("Run SQL"):
        try:
            if engine == "SQLite":
                cursor.execute(sql)
                if cursor.description is None:
                    raise ValueError("The model did not generate a result-producing query.")
                columns = [item[0] for item in cursor.description]
                rows = cursor.fetchmany(100)
            else:
                result = execute_sqlite_query(db_path, sql)
                columns = result["columns"]
                rows = result["rows"][:100]
            st.caption(f"Showing up to 100 rows from {engine}.")
            st.dataframe(pd.DataFrame(rows, columns=columns))
        except (sqlite3.Error, RuntimeError, ValueError, OSError) as error:
            st.error(f"{engine} could not run the generated query: {error}")

# Close the database connection
connection.close()
