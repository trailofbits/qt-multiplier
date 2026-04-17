# Cell Content Format

Spreadsheet cells hold typed content serialized as JSON with a "t" discriminator.

## Cell Types

### String: `{"t":"s","v":"text"}`
Use `write_cell(sheet_id, row, col, "text")`.

### Checkbox: `{"t":"b","v":1}` or `{"t":"b","v":0}`
Use `set_checkbox(sheet_id, row, col, true)`.

### Location (clickable code link): `{"t":"loc","e":entity_id,"p":"file.c","l":42,"c":10,"o":"..."}`
Use `write_location_cell(sheet_id, row, col, entity_id)`.
The tool resolves the entity to get file/line automatically. You only need the entity_id.
The column must be clickable for navigation to work (template sheets set this up).

### Document link: `{"t":"doc","id":7,"tl":"Title"}`
Use `link_document_to_cell(sheet_id, row, col, doc_id)`.

### Token (syntax-highlighted): `{"t":"tok","k":kind,"c":category,"d":"text","e":entity_id}`
Created by pasting from the code explorer, not typically by agent tools.

### Token Range: `{"t":"tr","v":[{token},{token},...]}`
Multiple syntax-highlighted tokens. Created by pasting code snippets.

## Creating Clickable Code Links

The simplest way to create a clickable code reference:
1. Get an entity_id from search_entities, get_references, get_callers, etc.
2. Call `write_location_cell(sheet_id, row, col, entity_id)`
3. The tool resolves everything — file path, line number, column — from the entity.

You do NOT need to construct LocationCell JSON manually. Just pass the entity_id string.

## Workflow: Recording a Finding

1. `create_findings_sheet()` — creates sheet with clickable Location column
2. `add_row(sheet_id, [...])` — add a row
3. `write_location_cell(sheet_id, row, 0, entity_id)` — clickable location
4. `write_cell(sheet_id, row, 1, "Finding description")` — text
5. `create_document("Detailed analysis", "...")` → doc_id
6. `link_document_to_cell(sheet_id, row, 3, doc_id)` — evidence link
7. `set_row_color(sheet_id, row, "red")` — critical finding
