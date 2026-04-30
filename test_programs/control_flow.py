# If/elif/else
x = 42
if x > 100:
    print("big")
elif x > 50:
    print("medium")
elif x > 10:
    print("small-medium")
else:
    print("small")

# While with break
i = 0
while True:
    i = i + 1
    if i > 5:
        break
    print(i)

# For with continue
for i in range(10):
    if i % 2 == 0:
        continue
    print(i)
