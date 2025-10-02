```
This "should"[^6]
"be"[^2] ignored
```

"This"[^4] ``"is"[^2] ignored``

but ``this`` is ``not`` "ignored",[^3] ``see?``

    ```md
    "This"[^1] ``should`` also
    "be"[^3] ignored
    ```

[^3]: A
[^2]: B
[^4]: C

```
This "should also be ignored. Despite not having a closing fence, Markdown treats everything below an opening fence as part of a code block"[^6]