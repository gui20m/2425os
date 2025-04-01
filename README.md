# 2425os

## Description
A client-server system for indexing/managing text documents with metadata. Includes automated testing script.

## Prerequisites
- GNU Make
- GCC compiler
- Bash shell

## Installation
**Compiling**
```console
make clean && make
```
**Automated Testing files + Running Server**
```console
bash setup.sh
```
**Automated Testing Commands**
```console
bash commands.sh
```
**Client task**
```css
./bin/dclient -<option> <[args1],...>
```