# Regex Interpreter in C++ using Brzozowski derivatives
This program inplements a regular expressions interpreter in C++ using Brzozowski derivative method.
It can evaluate whether a given string matches a specified regular expression at runtime without converting the regex to a full NFA/DFA.
## Features
Supports the following operators:
* \*
* \+
* \?
* \|
* \(\)
* \\

## Usage
```bash
$ ./main.out "<Regex>" "<String>"
```

### Examples
```bash
$ /main.out "a*b" "ab"
true
$ ./main.out "(a|b)+abb?" "abababb"
true
$./main.out "(a|b)+abb?" "ababa"
false
```
