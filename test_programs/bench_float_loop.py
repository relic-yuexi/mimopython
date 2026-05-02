x = 1.0
for i in range(1000000):
    x = x * 1.000001 + 0.000001
print(x)
