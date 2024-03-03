# qcDB
Quick Compact Database

## Schema
Schema's are .skm files which describe the data in a database.
Each schema represents an object. An object can represent many different things
that have attributes. This could be something like a Person, a car, a computer,
etc...

An object is made up of a name, a number, and fields. The object name describes
an object. The number can be used to uniquely identify one object from another
if multiple objects are in the same executable. The fields are data that make up
aspects of an object.

A field is made up of a name, an index, a data type, and a count. The name
describes the field. The index is the order of the field which start at 0. The
data type is one of several choices:

i: unsigned int -- -2147483648 to 2147483648
I: signed int -- 0 to 4294967295
l: unsigned long -- -4294967294 to 4294967294
L: signed long -- 0 and bigger positive numbers
?: boolean -- true/false 1/0
c: character -- a signed byte ASCII chars
b: byte -- an unsigned byte
x: padding -- extra bytes added to a struct by the compiler. Try to minimize

# Schema example
A schema starts with the number, the name, and number of records of an object

#OBJECT NUMBER, OBJECT NAME, NUMBER OF RECORDS
2 PERSON 100

Then the fields are listed
#INDEX NAME TYPE SIZE
0 NAME c 140
1 AGE i 1
2 GLASSES b 1

In this example of fields the NAME is the 0th field and it is an array of 140
characters. This represents a string, like a Tweet. The 1sth field is the Age
of the person. A single number between 0 and 2147483648. The last, 2nd index
is a single true/false entry if the PERSON has glasses or not.

Comments start with a #
#This is a comment

All together a PERSON.skm looks like this:

#Schema example for PERSON
2 PERSON 100
    0 NAME c 140
    1 AGE i 1
    2 GLASSES b 1
