import sqlite3
import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python"))
from hyperdb_bridge import execute_sqlite_query


class BridgeTests(unittest.TestCase):
    def test_filter_group_join_and_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.sqlite"
            with sqlite3.connect(path) as connection:
                connection.executescript("""
                    CREATE TABLE Department (id INTEGER, name TEXT);
                    CREATE TABLE Employee (dept_id INTEGER, name TEXT, salary REAL);
                    CREATE TABLE Misc (id INTEGER, payload BLOB);
                    INSERT INTO Department VALUES (1, 'Finance'), (2, 'Biology');
                    INSERT INTO Employee VALUES
                        (1, 'Ada', 10.0), (1, 'Lin', 20.0), (2, 'Sam', NULL);
                    INSERT INTO Misc VALUES (-5, x'00ff');
                """)
            result = execute_sqlite_query(path,
                "SELECT Department.name, COUNT(*), SUM(Employee.salary) "
                "FROM Department JOIN Employee ON Department.id = Employee.dept_id "
                "GROUP BY Department.name ORDER BY Department.name")
            self.assertEqual(result["columns"],
                ["Department.name", "COUNT(*)", "SUM(Employee.salary)"])
            self.assertEqual(result["rows"],
                [["Biology", 1, None], ["Finance", 2, 30.0]])
            result = execute_sqlite_query(path,
                "SELECT name FROM Department WHERE id = 2")
            self.assertEqual(result["rows"], [["Biology"]])
            self.assertEqual(execute_sqlite_query(path,
                "select NAME from department where ID = 2")["rows"], [["Biology"]])
            self.assertEqual(execute_sqlite_query(path,
                "SELECT id, payload FROM Misc WHERE id = -5")["rows"], [[-5, "0x00ff"]])
            with self.assertRaisesRegex(RuntimeError, "Expected SELECT"):
                execute_sqlite_query(path, "DELETE FROM Department")


if __name__ == "__main__":
    unittest.main()
