# Cell Content Format

Spreadsheet cells hold typed content serialized as JSON with a "t" discriminator.

## Cell Types

### String: `{"t":"s","v":"text"}`
Use `write_cell(sheet_id, row, column_name="Description", "text")`.

### Checkbox: `{"t":"b","v":1}` or `{"t":"b","v":0}`
Use `set_checkbox(sheet_id, row, column_name="Done", true)`.

### Location (clickable code link): `{"t":"loc","e":entity_id,"p":"file.c","l":42,"c":10,"o":"..."}`
Use `write_location_cell(sheet_id, row, column_name="Location", entity_id)`.
The tool resolves the entity to get file/line automatically. You only need the entity_id.
The column must be clickable for navigation to work (template sheets set this up).

### Document link: `{"t":"doc","id":7,"tl":"Title"}`
Use `link_document_to_cell(sheet_id, row, column_name="Evidence", doc_id)`.

### Token (syntax-highlighted): `{"t":"tok","k":kind,"c":category,"d":"text","e":entity_id}`
Created by pasting from the code explorer, not typically by agent tools.

### Token Range: `{"t":"tr","v":[{token},{token},...]}`
Multiple syntax-highlighted tokens. Created by pasting code snippets.

## Named Columns and Key Columns

**Always use column names instead of indices.** This is resilient to column reordering.

```
write_cell(sheet_id, row=0, column_name="Description", value="my text")
read_cell(sheet_id, row=0, column_name="Status")
```

**Key columns** provide dictionary-style access. A sheet's key column (e.g. "ID") lets you address rows by key value instead of index:

```
read_row_by_key(sheet_id, key="T-5")
update_row_by_key(sheet_id, key="T-5", values={"Status": "completed", "Notes": "done"})
```

**Key templates**: When adding rows, use `T-%` in the key column and the system auto-increments:
```
add_row(sheet_id, values={"ID": "T-%", "Description": "New task"})
→ returns {"key": "T-6", "row_index": 5}
```

## Schema Introspection

Use `get_sheet_schema(sheet_id)` before writing to understand the columns:
```json
{
  "columns": [
    {"index": 0, "name": "ID", "is_key": true, "clickable": false},
    {"index": 1, "name": "Location", "is_key": false, "clickable": true},
    {"index": 2, "name": "Finding", "is_key": false, "clickable": false}
  ],
  "key_column": "ID",
  "role": "findings"
}
```

## Workflow: Recording a Finding

```
create_findings_sheet()
add_row(sheet_id, values={
    "Location": "",  # placeholder, will be overwritten
    "Finding": "Buffer overflow in parse_header",
    "Severity": "high",
    "Status": "new"
})
write_location_cell(sheet_id, row=0, column_name="Location", entity_id="12345")
create_document("parse_header analysis", "Detailed analysis...")  → doc_id
link_document_to_cell(sheet_id, row=0, column_name="Evidence", doc_id)
set_row_color(sheet_id, row=0, "red")
```

## Annotated Code Blocks

In conversation messages, annotate code fences for syntax-highlighted rendering:

    ```fragment:ENTITY_ID
    code fallback here
    ```

    ```entity:ENTITY_ID
    int parse_header(const char *buf) { ... }
    ```
