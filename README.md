# citeorder
[![C Unit Tests](https://github.com/dhanushka2001/citeorder/actions/workflows/main.yml/badge.svg)](https://github.com/dhanushka2001/citeorder/actions/workflows/main.yml)

Simple command-line tool to correctly order Footnotes in Markdown files

## How to use

1. On Windows, simply download the precompiled executable from the latest release

   If you want to compile the source code yourself, clone the repo and compile ``citeorder.c`` (including flags like ``-std=c11`` may cause errors):

   ```console
   git clone https://github.com/dhanushka2001/citeorder
   ```
   
   ```console
   gcc -Wall citeorder.c -o citeorder
   ```
2. To run, simply enter into the terminal:

   ```console
   citeorder input.md
   ```

   where ``input.md`` is the Markdown file whose Footnotes you want reordered. ``citeorder`` will keep the original file as is and output the changes on a new file, ``input-fixed.md``.

## Example

``example.md``:

```md
"Alice says hi".[^1]

[^1]: Alice

"Bob is here".[^7] "I'm Charlie",[^4] "Daniel!",[^5] here.

[^4]: Charlie
[^3]: Gary
[^5]: Daniel
[^7]: Bob

Is "Ethan"[^8] here?

[^8]: Ethan

"Bob and Charlie here again"[^7][^4]

[^6]: Fred
```

Running:

```console
citeorder example.md
```

will produce ``example-fixed.md``:

```md
"Alice says hi".[^1]

[^1]: Alice

"Bob is here".[^2] "I'm Charlie",[^3] "Daniel!",[^4] here.

[^2]: Bob
[^3]: Charlie
[^4]: Daniel
[^6]: Gary

Is "Ethan"[^5] here?

[^5]: Ethan

"Bob and Charlie here again"[^2][^3]

[^7]: Fred
```

The Markdown processor automatically reorders Footnotes when converting ``.md`` files, however, using ``citeorder`` will fix the ordering in the text file itself, making the file neater and easier to manage long lists of footnotes, especially useful when needing to add new footnotes in the middle of a long ``.md`` file and not having to reorder every in-text and full-entry citation manually.

Full-entry citations (``[^1]: Alice``) and in-text citations (``"Alice here",[^1]``) are a one-to-many relationship. ``citeorder`` assumes the connections are correct and **sorts them according to the order in which the in-text citations appear.**

``citeorder`` handles cases like:

* No changes needed.
* Stacked in-text citations, e.g. ``"hello",[^3][^1]`` → ``"hello",[^1][^2]``.
* Punctuation (or no punctuation) after the quote, e.g. ``Say "A"[^3] and "B",[^2]`` → ``Say "A"[^1] and "B",[^2]``
* Missing numberings; ``citeorder`` will push all numbers above the missing number(s) down to fill the gap(s).
* Full-entry citations with no matching in-text citations simply get bubbled to the top of the ordering.
* Error handling for improper quote, e.g. ``"hello[^1]`` produces a warning message like: ``WARNING: in-text citation [^1] not properly quoted (line 5)``.
* Error handling for duplicate full-entry citation numberings, e.g.

  ```md
  [^4]: Alice
  [^4]: Bob
  ```
  
  produces an error, like: ``ERROR: duplicate [^4] full-entry citations (line 7 and 8)``.
