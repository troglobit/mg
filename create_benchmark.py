with open("benchmark.txt", "w") as f:
    line = "ababab" * 100 + "\n"  # A long line with a repeating pattern
    for _ in range(10000):  # 10000 lines
        f.write(line)
