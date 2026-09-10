# Adaptation Programming Guide

Continue to the maintained [Adaptation Programming Guide](../docs/programming-guide.md).

## Additional patch text views

The optional `getClearText(message)` callback returns a list of `(name, text)` string
pairs. The input is the complete stored patch data, in the same format as
`nameFromDump`, including SysEx framing and any concatenated messages. The adaptation
is responsible for extracting and decoding the payload.

```python
def getClearText(message):
    if not isEditBufferDump(message):
        return []
    data = unescapeSysex(message[5:-1])  # Use your synth's framing here.
    text = "\n".join(
        f"{offset:04x} " + " ".join(f"{b:02x}" for b in data[offset:offset + 8])
        for offset in range(0, len(data), 8)
    )
    return [("Unescaped patch data", text)]
```

Views appear in the patch comparison and current-patch sidebar selectors. Each
name must be nonempty, unique within the result, and stable across patches.
Comparison offers names present on both sides and matches them by name, independent
of return order. Return `[]` for unsupported patch types or when no views apply.
Omitting the callback preserves the built-in views; a callback error is logged and
also falls back to those views.

Text is displayed verbatim in a fixed-width, non-wrapping editor: spaces, tabs,
blank lines, line endings, and trailing newlines are preserved. Resizing does not
reformat it. Use consistent line ordering and widths for comparable patches. The
comparison highlights text changes without inserting alignment lines or changing
your layout. When both texts have the same number of rows, corresponding rows are
compared independently to keep small changes in long dumps localized. When row
counts differ, JUCE compares the complete texts. Additional representations can
show decimal bytes, parameter values,
layers, or synth-specific offset reports. No generic mapping between packed and
unpacked byte indices is inferred.
