# SQLite tools for Python
import sqlite3

# Location of our database
DB_PATH = "data/hyperdb.db"


# Get table and column names
def get_schema():

    # Open the database
    connection = sqlite3.connect(DB_PATH)

    # Tables we want to inspect
    tables = ["rates", "plan_attributes"]

    # Store the schema here
    schema = {}

    # Process each table
    for table in tables:

        # Get information about its columns
        cursor = connection.execute(f"PRAGMA table_info({table})")

        # Save the column names
        schema[table] = [row[1] for row in cursor.fetchall()]

    # Close the database
    connection.close()

    # Return the schema
    return schema


# Run this only when the file is executed directly
if __name__ == "__main__":
    question = input("Ask HyperDB: ")
    print(question)
    # Get the database schema
    schema = get_schema()

    # Process each table
    for table, columns in schema.items():

        # Print the table name
        print(f"\n{table}:")

        # Print each column
        for column in columns:
            print(f"  - {column}")
