```
This "should"[^6]
"be"[^2] ignored
```

"This"[^1] ``"is"[^2] ignored``

but ``this`` is ``not`` "ignored",[^2] ``see?``

    ```md
    "This"[^1] ``should`` also
    "be"[^3] ignored
    ```

[^1]: C
[^2]: A
[^3]: B

```
This "should also be ignored. Despite not having a closing fence, Markdown treats everything below an opening fence as part of a code block"[^6]