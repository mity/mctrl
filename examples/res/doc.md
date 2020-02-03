
# Example Markdown Document

## What is Markdown

> Markdown is a lightweight markup language with plain text formatting syntax.
> Its design allows it to be converted to many output formats, but the original
> tool by the same name only supports HTML. Markdown is often used to format
> readme files, for writing messages in online discussion forums, and to create
> rich text using a plain text editor.

*(Source: https://en.wikipedia.org/wiki/Markdown)*


## What is MD4C

[MD4C](https://github.com/mity/md4c) is C Markdown parser implementation used
for parsing the Markdown documents. This parser is used for the purposes of the
Markdown view control.

MD4C is compliant to [CommonMark specification](http://spec.commonmark.org/),
and it also implements some commonly used extensions to it.

**Note:** Parsing of raw HTML blocks and raw HTML inlines are disabled: We do
not want to implement HTML parser and renderer in the Markdown view control.


## About This Document

This example document lives as a resource embedded in the program. The Markdown
view control is able to display such documents. It can be pointed with the URL
[`res://example-mdview.exe/doc.md`](res://example-mdview.exe/doc.md).

You may see the document in its raw form in `mCtrl/examples/res/doc.md`, both
in the source or binary package of mCtrl.


## Markdown Syntax Overview

The following sections describes the Markdown syntax (as of supported by the
Markdown Viewer control).

Note this is by no means a complete documentation of all Markdown features.

Refer e.g. to https://spec.commonmark.org/0.29/ for more detailed description
of core Markdown features and to https://github.github.com/gfm/ for Markdown
extensions like task lists, tables, permissive auto-links or strike-through
text spans.

Note however that some features, in particular the raw HTML spans and raw HTML
blocks, are not supported in this control and will be rather rendered verbatim
as an ordinary text.

### Headings

To create a heading, add number signs (`#`) in front of a word or phrase. The
number of number signs you use should correspond to the heading level.

Example:

> ``` Markdown
> # Heading (level 1)
> ## Heading (level 2)
> ### Heading (level 3)
> #### Heading (level 4)
> ##### Heading (level 5)
> ###### Heading (level 6)
> ```
>
> # Heading (level 1)
> ## Heading (level 2)
> ### Heading (level 3)
> #### Heading (level 4)
> ##### Heading (level 5)
> ###### Heading (level 6)

Alternatively, on the line below the text, add any number of `=` characters for
heading level 1 or `-` characters for heading level 2.

> ``` Markdown
> Heading (level 1)
> =================
>
> Heading (level 2)
> -----------------
> ```
>
> Heading (level 1)
> =================
>
> Heading (level 2)
> -----------------

### Paragraphs

To create paragraphs, use a blank line to separate one or more lines of text.
You should not indent paragraphs with spaces or tabs:

> ``` Markdown
> Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Nullam eget nisl.
> Aenean vel massa quis mauris vehicula lacinia. Phasellus et lorem id felis
> nonummy placerat. Mauris dolor felis, sagittis at, luctus sed, aliquam non,
> tellus. Etiam neque. In dapibus augue non sapien. Nulla est.
>
> Nullam lectus justo, vulputate eget mollis sed, tempor sed magna. Aliquam erat
> volutpat. Maecenas aliquet accumsan leo. Integer in sapien. Duis risus. Quis
> autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil
> molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla
> pariatur? Praesent dapibus.
> ```
>
> Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Nullam eget nisl.
> Aenean vel massa quis mauris vehicula lacinia. Phasellus et lorem id felis
> nonummy placerat. Mauris dolor felis, sagittis at, luctus sed, aliquam non,
> tellus. Etiam neque. In dapibus augue non sapien. Nulla est.
>
> Nullam lectus justo, vulputate eget mollis sed, tempor sed magna. Aliquam erat
> volutpat. Maecenas aliquet accumsan leo. Integer in sapien. Duis risus. Quis
> autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil
> molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla
> pariatur? Praesent dapibus.

### Line Breaks

To enforce a line break inside a paragraph, end a line with two or more spaces;
or end the line with a backslash `\` character.

> ``` Markdown
> This is the first line. \
> And this is the second line.
> ```
>
> This is the first line. \
> And this is the second line.

### Emphasis

Markdown supports bold and italic spans in the text denoted with `*` or `_`
characters (bold span needs two of them, italic just one). The difference is
that the underscore has no special meaning when used inside a word, while
asterisk can be used to emphasize just part of a word.

Similarly, strike-through spans are denoted with `~`.

> ``` Markdown
> This sentence combines **bold** (__bold__), *italic* (_italic_) and
> ~strike-through~ spans. The emphasis can even be combined together, so that
> for example ***bold italic*** or ~*strike-through italic*~ text is also
> possible.
> ```
>
> This sentence combines **bold** (__bold__), *italic* (_italic_) and
> ~strike-through~ spans. The emphasis can even be combined together, so that
> for example ***bold italic*** or ~*strike-through italic*~ text is also
> possible.

### Entities

It is possible to use HTML-like entities.

> ``` Markdown
> Example of entities: &nbsp; &amp; &copy; &AElig; &Dcaron; &frac34;
> &HilbertSpace; &DifferentialD; &ClockwiseContourIntegral; &ngE;
> ```
>
> Example of entities: &nbsp; &amp; &copy; &AElig; &Dcaron; &frac34;
> &HilbertSpace; &DifferentialD; &ClockwiseContourIntegral; &ngE;

### Links

There are several ways how to write a link. Inline links are written as `[link
body](link destination)`.

URLs are also recognized when enclosed inside `<` and `>`. Simple `http:` and
`https:` URLs without special characters are recognized even without the
brackets.

> ``` Markdown
> Visit [mCtrl website](https://mctrl) or the project repository located at
> https://github.com/mity/mctrl.
> ```
>
> Visit [mCtrl website](https://mctrl) or the project repository located at
> https://github.com/mity/mctrl.

### Images

TBD.

### Lists

Markdown supports unordered lists where the bullets are denoted with any of
`*`, `-` or `+` characters.


> ``` Markdown
> * One
> * Two
> * Three
>
> - One
> - Two
> - Three
> ```
>
> * One
> * Two
> * Three
>
> - One
> - Two
> - Three

Similarly, ordered lists are created by using a number followed by `)` or `.`.

> ``` Markdown
> 1. One
> 2. Two
> 3. Three
>
> 1) One
> 2) Two
> 3) Three
> ```
>
> 1. One
> 2. Two
> 3. Three
>
> 1) One
> 2) Two
> 3) Three

### Blockquotes

Blockquotes are created by prepending the lines with `>` character.

> ``` Markdown
> > This is a blockquote.
> ```
>
> > This is a blockquote.

### Horizontal Rules

Horizontal rules are created by three or more `-` or `*` characters. The lines
cannot contain anything else (except a whitespace).

> ``` Markdown
> Some sections of the document can be delimited with a horizontal rule.
>
> ***
>
> But please, use them sparingly. ;-)
> ```
>
> Some sections of the document can be delimited with a horizontal rule.
>
> ***
>
> But please, use them sparingly. ;-)

### Inline Code

Inline code can be written by using `` ` `` characters.

> ``` Markdown
> This is `an inline code`.
> ```
>
> This is `an inline code`.


### Code Blocks

Code blocks can be created by wrapping it in fence lines made of three or more
`` ` `` or `~` characters.


> ``` Markdown
> ~~~
> #include <stdio.h>
>
> int
> main(int argc, char** argv)
> {
>     printf("Hello world!\n");
>     return 0;
> }
> ~~~
> ```
>
> ~~~
> #include <stdio.h>
>
> int
> main(int argc, char** argv)
> {
>     printf("Hello world!\n");
>     return 0;
> }
> ~~~
