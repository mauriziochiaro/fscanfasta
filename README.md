# fscanfasta

A tiny experiment comparing two approaches to parsing structured data:
1. Standard C library's `fscanf()`
2. Custom in-memory buffer parsing
3. C++ centrilized function with scanf-like format string

## What is this?

fscanfasta demonstrates how reading structured data directly from memory can be faster than traditional file I/O with stdio functions. The code implements a common pattern for efficient data processing: load everything into RAM first, then parse.

## Results

With a ~300MB test file:

```
fscanf: 3320264 record read in 8.370 seconds (2.521 usec/record)
fscanfasta[C]: 3320264 record read in 2.132 seconds (0.642 usec/record)
fscanfasta[C++]: 3320264 record read in 3.110 seconds (0.937 usec/record)
```

Memory buffer parsing is ~3x/4x faster on typical hardware.

## How to use

Just compile and run:

```
gcc -o fscanfasta fscanfasta.c
./fscanfasta
```

For the C++ implementation:

```
cl /EHsc /O2 /std:c++20 fast_fscanf.cpp fscanfasta.c /Fe:fscanfasta.exe
./fscanfasta
```

The program will:
1. Generate a test file (`testdata.txt`) if it doesn't exist
2. Compare parsing speed using the 3 methods

## Why?

Because sundays are boring.

## License

MIT, do whatever you want.