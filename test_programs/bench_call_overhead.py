def inc(x):
    return x + 1

x = 0
for i in range(500000):
    x = inc(x)
print(x)
