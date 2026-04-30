def add(a, b):
    return a + b

def factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)

def greet(name):
    print("Hello,", name + "!")

print(add(3, 4))
print(factorial(10))
greet("World")

# Higher-order usage
def apply_twice(f, x):
    return f(f(x))

def double(x):
    return x * 2

print(apply_twice(double, 3))
