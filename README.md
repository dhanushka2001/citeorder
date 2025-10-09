<img width="640" height="320" alt="citeorder-logo-light" src="https://github.com/user-attachments/assets/21bdb71e-fbe1-4c4b-8f8d-5cd2d4f1fca7" />

# citeorder
[![Build and Test](https://github.com/dhanushka2001/citeorder/actions/workflows/main.yml/badge.svg)](https://github.com/dhanushka2001/citeorder/actions/workflows/main.yml)
[![GitHub Release](https://img.shields.io/github/v/release/dhanushka2001/citeorder)](https://github.com/dhanushka2001/citeorder/releases)
[![GitHub License](https://img.shields.io/github/license/dhanushka2001/citeorder)](https://github.com/dhanushka2001/citeorder/blob/main/LICENSE)
[![GitHub commit activity](https://img.shields.io/github/commit-activity/m/dhanushka2001/citeorder)](https://github.com/dhanushka2001/citeorder/commits/main/)
[![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/dhanushka2001/citeorder/total)](https://github.com/dhanushka2001/citeorder/releases)
[![GitHub Repo stars](https://img.shields.io/github/stars/dhanushka2001/citeorder)](https://github.com/dhanushka2001/citeorder/stargazers)

![C](https://img.shields.io/badge/C-%2300599C.svg?style=plastic&logo=c&logoColor=white)
![Markdown](https://img.shields.io/badge/Markdown-%23000000.svg?style=plastic&logo=markdown&logoColor=white)

<!--
![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)
![Markdown](https://img.shields.io/badge/markdown-%23000000.svg?style=for-the-badge&logo=markdown&logoColor=white)
-->

Simple command-line tool to correctly reorder Footnotes in Markdown files.

## Motivation

Markdown processors that support footnotes (e.g. [GitHub’s Markdown engine](https://github.com/github/cmark-gfm), which implements the [GitHub Flavored Markdown](https://github.github.com/gfm) spec) automatically reorder footnotes when converting ``.md`` files to HTML. However, ``citeorder`` fixes the ordering in the ``.md`` file itself, making it neater and easier to manage lots of footnotes. Especially useful when needing to add new footnotes in the middle of a long ``.md`` file and not having to spend ages reordering every in-text and full-entry footnote manually (🥲).

In-text footnotes (``"Alice here",[^1]``) and full-entry footnotes (``[^1]: Alice``) are a many-to-one relationship. ``citeorder`` assumes the connections are correct, and relabels them according to the order in which the **in-text footnotes** appear.

## How to use

1. On Windows, simply download the precompiled executable from the latest [release](https://github.com/dhanushka2001/citeorder/releases).

   Or, if you want to compile the source code yourself, clone the repo and compile ``citeorder.c``:

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

   where ``input.md`` is the Markdown file whose Footnotes you want reordered. ``citeorder`` will keep the original file as is and output the changes to a new file, ``input-fixed.md``.

   To allow relaxed quote handling, do:

   ```console
   citeorder -r input.md
   ```

   For more info and options, run:

   ```console
   citeorder -h
   ```

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

## Cases handled

* No changes needed.
* Stacked in-text footnotes, e.g. ``"hello",[^3][^1][^5]`` → ``"hello",[^1][^2][^3]``.
* Single punctuation (or none) after the quote, e.g. ``"A"[^3] "B",[^2] "C".[^6] "D"![^5]`` → ``"A"[^1] "B",[^2] "C".[^3] "D"![^4]``.
* Improper quote, e.g. ``"hello[^1]``, ``"hello",,[^1]``, ``hello"[^1]``, ``"hello" [^1]`` produces an error message like: ``ERROR: in-text citation [^1] not properly quoted (line 5)``. Can ignore this error with the ``-r``/``--relaxed-quotes`` flag.
* Full-entry footnotes with no matching in-text footnotes simply get bubbled to the end of the ordering.
* In-text footnotes with no matching full-entry footnote produce an error message like: ``ERROR: in-text citation [^2] without full-entry (line 3)``.
* Duplicate full-entry footnotes, e.g.

  ```md
  [^4]: Alice
  [^4]: Bob
  ```
  
  produces an error message like: ``ERROR: duplicate [^4] full-entry citations (line 7 and 8)``.
* Footnotes inside inline code (``"A"[^1]``) and fenced code blocks:

  ```md
  "A"[^1]
  ```

  are ignored.
* Footnote labels with letters/symbols are supported, and will be relabeled accordingly, e.g. ``"A"[^6b]`` → ``"A"[^1]``.
* Spaces in the in-text or full-entry footnotes. Spaces outside the label for in-text footnotes, e.g. ``"A"[^  Alice ]`` is accepted by Markdown processors, and ``citeorder`` will convert that to ``"A"[^1]``. However, for full-entry footnotes, e.g. ``[^ 4b  ]: Alice`` it is not accepted, and in ``citeorder`` it will produce an error message like: ``ERROR: [^ 4b  ] full-entry citation contains a space (line 3)``. For both in-text and full-entry footnotes, spaces in the label itself, e.g. ``"A"[^4 b]``, ``[^4 b]: Alice``, are not accepted, and in ``citeorder`` you will get an error message.
* In-text or full-entry footnote missing label, e.g. ``"A"[^]``, will produce an error message like: ``ERROR: in-text citation [^] missing label (line 7)``.
