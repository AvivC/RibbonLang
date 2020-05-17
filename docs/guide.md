# Programming in Ribbon

This page is a general guide for writing code in Ribbon. Please go through the [README](../README.md) before reading this guide.

To see an example of a real use of Ribbon, please check out [the source code for this 2D game written entirely in Ribbon](../src/game).

### Datatypes

Ribbon has two types of data types - values and objects. Values are immutable and live on the stack. Objects are often mutable,
live on the heap and are managed by the garbage collector. New object types can be defined as a new class by the programmer.

Instances of all types are treated using the same syntax.

Builtin value types:

* Number
* Boolean
* Nil

Builtin object types:

* String
* Table
* Function
* Class
* Module

#### Number, Boolean and Nil

```
# This is a comment

number = 30.5  # Can be an integer or a floating point
bool = true  # true or false
my_nil = nil  # represents no value
```

#### String

Strings are immutable objects. We can perform different operations on them, among concatenation and accessing individual characters.
Every string operation returns a new string.

```
string = "Hello Ribbon!"
second_char = string[1]
string_length = string.length()
longer_string = string + " Some more words"
```

#### Table

In Ribbon, Tables are used both as list-style structures or and as dictionary containers. It is a hashtable under the hood.

Table used as a list:

```
list = ["these", "are", "some", "items"]

print(list[1])  # prints "are"
print(list.length())  # prints 4
list.add("another item")
list.pop()  # remove last item

for item in list {
    print(item + ", ")  # > these, are, some, items
}
```

Table used as a dictionary

```
dictionary = ["python": "elegant", "javascript": "nice", "ribbon": "great"]
print(dictionary["ribbon"])  # > great
```

### User input

Ribbon comes with a variety of builtin functions (along with a few standard library modules - more on this later).

We will not cover all of them here. Please take a look at the source code in `vm.c` and `builtins.c` to learn more.

The `print()` builtin function, well, prints a string to the terminal. The `input()` builtin takes input from the user
and returns it as a string.

```
print("What is your name?")
name = input()
print("Hello, " + name)
```

### Control flow

Ribbon has the classic `if`, `while` and `for` statements that we know and love.

#### If, Elsif and Else

```
if some_condition() {
    # ...
} elsif other_condition() {
   # ...
} else {
   # ...
}
```

#### While

```
while some_condition() {
   # ...
}
```

#### For

`for` takes any object that implements (borrowing the terminology from Python) the sequence protocol.
That includes the methods `length` and `@get_key` (the `[]` accessor).

Table objects are an example of an iterable object:

```
numbers = [10, 20, 30]

sum = 0
for n in numbers {
    sum += n
}

print("The sum is: " + to_string(sum))
```

### Functions

In Ribbon, all functions are basically lambdas. We can assign a function to a variable in order to give it a name.

```
empty_function = {}  # No need to specify a parameter list if there aren't any

multiply = { | x, y |
    return x * y
}
```

Like most things in Ribbon, functions are first class - we can pass them into other functions, in a functional programming style.

```
# Example of a higher order function
map = { | list, func |
    result = []
    for item in list {
        result.add(func(item))
    }
    return result
}

# Passing an anonymous function into map. This returns [1, 4, 9]
map([1, 2, 3], { | n |
    return n * n
})
```

Functions in Ribbon are **closures** - they can remember the surrounding variables from the scope they were defined in.
This allows neat things such as:

```
greet_maker = { | name |
    return {
        return "Hello, " + name + "!"
    }
}

my_greet = greet_maker("Ribbon")
my_greet()  # "Hello, Ribbon!"
```

### Classes

Class declarations, like functions, are simply anonymous expressions. Like all expressions, they can be assigned to variables.

```
Dog = class {
    # constructor is optional
    @init = { | name |
        self.name = name
    }
    
    bark = {
        print(self.name + ": Woof!")
    }
}

rex = Dog("Rex")
brown = Dog("Brown")
rex.bark()  # "rex: Woof!"
brown.bark()  # "brown: Woof!"
```

Ribbon has single inheritance:

```
Labrador = class : Dog {
    bark = {
        # Override superclass bark()
        print(self.name + ": Ruff!")
    }
    
    play = { | ball |
        return ball
    }
}

Shepard = class : Dog {
    @init = { | name |
        # Override superclass constructor
        
        name += " the shepard"
        super([name])  # call superclass constructor
    }
    
    # Inherit bark() from superclass
}

milki = Labrador("Milki")
barak = Shepard("Barak")

milki.bark()  # Milki: Ruff!
milki.play("the ball")  # returns "the ball"
barak.bark()  # Barak the shepard: Woof!
```

### Polymorphism

Polymorphism in Ribbon is based on "duck typing" - if it quacks like a duck, it's a duck.

Meaning, a method call on an object is resolved at runtime. If it has a matching method, that method is invoked.
Inheritance isn't needed to acheive polymorphism.

```
Wolf = class {
    bark = {
        print("Wolf: ahoooo")
    }
}

dogs = [Labrador("Milki"), Shepard("Barak"), Wolf()]

# prints:
#     Milki, Ruff!
#     Barak the shepard: Woof!
#     Wolf: ahoooo
for dog in dogs {
    dog.bark()
}
```

### Modules

Ribbon modules are other `.rib` code files that you can access from your file. After a module is imported, its global variables are
exposed for use in the importing module.

File `program.rib`:

```
import myutils

my_string = "Hello Ribbon"
tripple = myutils.multiply_string(my_string, 3)

print(tripple)
```

File `myutils.rib`:

```
multiply_string = { | string, n |
    result = ""
    i = 0
    
    while i < n {
        result += string
        i += 1
    }
    
    return result
}
```

Ribbon has a standard library of modules. When `import`ing a module, if one of a matching name can't be found next to your main program,
the module is searched in the standard library.

The standard library is currently very minimal. It consists of `math`, `path` and `graphics`.

* The `math` module offers basic math operations implemented directly in Ribbon, such as square root and power.
* The `path` module offers a few convenience functions for working with file paths.
* The `graphics` module facilitates 2D graphics programming in Ribbon. It is a native module written in C.

For example:

```
import math

print(math.sqrt(25))  # 5
```

### Boolean and arithmetic expressions

The arithmetic operators are:

```
Addition         +
Subtraction      -
Multiplicaiton   *
Division         /
Modulo           %
```

The comparison operators are:

```
Less than            <
Less than equals     <=
Greater than         >
Greater than equals  >=
Equals               ==
```

And the boolean relation operators are `and` and `or`. These are *short circuiting*.

For example:

```
a = 10
b = 20

if a * b > 100 or b % 2 == 0 {
    # Do something
}
```

### The extension system

The Ribbon interpreter is extensible through writing custom extension modules in C.

An extension module is visible to Ribbon code the same way as a normal module. And since it is written in C, it can access resources
and OS functions which may not be available directly to Ribbon code.

Ribbon includes an extension module named `graphics` in its standard library. [Its source code can be found here](../src/sdl_extension).
This guide doesn't go into detail on developing extension modules. Learning this can be done by studying the source code.

### Miscellaneous

#### Scopes

Ribbon has only two scopes - functions scope and file scope. As mentioned, functions can be nested. A variable is only available
in the scope it was defined in.

By default, setting a variable always shadows any outer scope that happens to have a variable of that name. For example:

```
x = 10

f = {
    x = 20  # Creates a new variable. Does not change the outer variable
    print(x)  # Prints 20
}

f()
print(x)  # Prints 10
```

In case you do need to set an outer variable inside a function, use the `external` keyword:

```
x = 10

f = {
    external x
    x = 20  # Sets the value of the outer variable
    print(x)  # Prints 20
}

f()
print(x)  # Prints 20. f() changed the value of x
```

-----

This concludes this guide for programming in Ribbon. There are additional builtin functions and corners of the language. Please
explore the source code to learn more.
