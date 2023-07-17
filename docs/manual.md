# Generating a new database

Locate the `mx-index` binary within your `Multiplier.app` bundle, then execute the following command:

`mx-index --target compile_commands.json --db /tmp/database.mx`

Note that it is advised to use the temporary folder in order to avoid potential memory mapping errors within the indexer.

# User interface

## Searching and filtering

All widgets support searching or filtering functionality by pressing the following key combinations:

|          | macOS         | KDE Plasma | GNOME                      |
|----------|---------------|------------|----------------------------|
| Find     | `Cmd+F`       | `Ctrl+F`   | `Ctrl+F`                   |
| Next     | `Cmd+G`       | `F3`       | `Ctrl+G`, `F3`             |
| Previous | `Cmd+Shift+G` | `Shift+F3` | `Ctrl+Shift+G`, `Shift+F3` |

## History

|         | macOS    | KDE Plasma | GNOME      |
|---------|----------|------------|------------|
| Back    | `Cmd+[` | `Alt+Left` | `Alt+Left` |
| Forward | `Cmd+]` | `Alt+Left` | `Alt+Left` |

## Code view

### Settings

 * Word wrapping: `View, Enable word wrap`
 * Code preview when hovering a token: `View, Code preview`

### Key bindings

With the text cursor on a token:

 * `I`: Open in the `Information Browser`
 * `X`: Show call hierarchy. See the `Reference Explorer` section
 * `T`: Show tainting. See the `Reference Explorer` section

## Entity Explorer

The entity explorer works like a full database search. Enter the search string and either select `Exact` or `Containing`.

Results can be further filtered either by category (using the combo box at the top) or with the searching and filtering shortcuts.

## Reference Explorer

### Basic usage
The reference explorer can be opened in two ways:

1. Right clicking on a token
2. With the text cursor on a token, pressing either `T` (for tainting) or `X` (to show up the call hierarchy).

Within the widget, there are three components that are always available:

* Graphical tree view
* Text representation
* Code preview

Under `View, Reference Explorer` it is possible to choose what is the default split between the graphical and text representation. Regardless of the setting, it is possible to show either view at any time by simply dragging the top or lower edge.

The code preview is always hidden by default, but can be shown by dragging the right edge toward the center. If it is visible, it will stay in sync with the selected item.

In the top right corner, the following three buttons are visible:

* Save to active tab: If there is a reference explorer docked at the bottom, then copy the references there and close the dialog. Otherwise, create a new tab.
* Save to new tab: Always created a new docked tab a the bottom containing all the references on screen.
* Close


