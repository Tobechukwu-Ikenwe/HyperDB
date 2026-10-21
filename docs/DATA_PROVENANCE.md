# Insurance data provenance and integrity

## What the repository contains

The local `data/` directory has loose `plan-attributes-puf.csv` and `rate-puf.csv` files, ZIP files with the same CSV filenames, and a SQLite database with `plan_attributes`, `rates`, and an unrelated `trips` table. The two PUF-like CSVs have `BusinessYear`, `StateCode`, `IssuerId`, `SourceName`, and `ImportDate` fields. This repository does not contain the original download URLs, download log, license acceptance record, or the code that built `data/hyperdb.db`.

CMS publishes [2026 Health Insurance Exchange PUFs](https://www.cms.gov/marketplace/resources/data/public-use-files). Its [Rate PUF dictionary](https://www.cms.gov/files/document/rate-datadictionary-py26.pdf) describes rate fields by age, tobacco use, and rating area; its [Plan Attributes dictionary](https://www.cms.gov/files/document/planattributes-datadictionary-py26.pdf) describes plan-level fields. Consult the [CMS data disclaimer and user agreement](https://www.cms.gov/files/document/exchange-pufs-disclaimagree-py26.pdf-0) before using the data. The local filenames and headers are consistent with these CMS file types, but **their exact origin and release revision have not been authenticated against a CMS checksum**. CMS states that 2026 PUFs may be updated, so a later download need not match the local copy.

## Verified local relationship

Run `python3 python/verify_insurance.py` from the repository root. It reads every row but does not modify any file. The 2026-10-20 run established:

| Local CSV / SQLite table | Rows | Columns | Loose CSV SHA-256 | ZIP member | SQLite contents |
| --- | ---: | ---: | --- | --- | --- |
| `plan-attributes-puf.csv` / `plan_attributes` | 22,059 | 151 | `ed14c79af7e3acef037053e8ac91b5bacaa932f6e6f186daf390d7858b0e9c76` | Byte-identical | Same header and every row in rowid order |
| `rate-puf.csv` / `rates` | 2,235,761 | 20 | `6f991b7130c04c5850238c5d3acb74e41b91ea5e65dacb82da185a9fab877e97` | Byte-identical | Same header and every row in rowid order |

These checks prove the three local representations agree for these two tables. They do not establish that the archives match a particular CMS release. `data/plan_attributes_2026.zip` is not a readable ZIP in this working tree; do not use it as a source archive.

## Schema and analysis limits

The imported SQLite columns are all declared `TEXT`, including `IndividualRate` and other numeric-looking rate fields. Parse amounts explicitly and decide how to handle empty strings, missing values, dates, categorical age bands, and effective periods before numerical analysis. A row is a plan/rating-area/rate-period record in the Rate PUF, not an enrollment or person-level observation. The PUF coverage excludes some state-based Exchanges, as described by CMS. Do not infer population-wide premiums or outcomes from raw row counts.

For a quick read-only count and schema preview, run `python3 python/inspect_insurance.py`. The C++ CLI and Python bridge deliberately do not import these large tables into HyperDB; the bridge has a 10,000-row-per-table cap. Use SQLite or a separate bounded sample for exploratory work.
