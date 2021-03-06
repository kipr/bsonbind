bsonbind
========


BSON bind is a utility that generates C++ structs for descriptions of BSON.

Dependencies
============
* cmake > 2.8
* c++ compiler with C++11 support

Building
========
```
mkdir build
cd build
cmake ..
make
make install
```

Useful cmake arguments:
* `-DCMAKE_INSTALL_PREFIX=<install prefex>`, recommended for Windows

File format
===========

```
 type1 key1
 # This key is required
 type2 key2!
 # This is an array
 type3[] key3
 
 # This is a sub-document. Note: this type must have its own description file and matching hpp header.
 # Generation of this description must be done by the user
 my_great_type key4
```

There are also special directives that can be used to set options for the file:

```
# Set the C++ namespace
%package name
# The bind method shall not be generated
%nobind
# The unbind method shall not be generated
%nounbind
```

Note: Empty lines and lines beginning with `#` are ignored.

The extension for these files shall be `bsonbind`.

Valid types
===========

All types may be suffixed with `[]` for dynamicly sized arrays

```
bool
int8
int16
int32
int64
uint8
uint16
uint32
uint64
real32
real64
string
```

*Note: A uint8[] is stored as a BSON binary blob*

Usage
=====

`bson_bind type.bsonbind type.hpp`

Note: The generated struct will have the same name as the header and will be in the `bson_bind` namespace


Generated structs
=================

All members in generated structs are public. Two functions, `bind` and `unbind`, will be generated in the struct:
  - `bind` will construct a bson_t from the struct
  - `unbind` (static) will construct a struct from a bson_t
