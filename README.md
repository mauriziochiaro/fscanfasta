# fscanfasta

A tiny experiment comparing two approaches to parsing structured data:
1. Standard C library's `fscanf()`
2. Custom in-memory buffer parsing

## What is this?

fscanfasta demonstrates how reading structured data directly from memory can be faster than traditional file I/O with stdio functions. The code implements a common pattern for efficient data processing: load everything into RAM first, then parse.

## Results

With a ~300MB test file:

```
fscanf: 3451936 record read in 5.887 seconds (1.705 usec/record)
custom: 3451936 record read in 1.455 seconds (0.422 usec/record)
```

Memory buffer parsing is ~3x/4x faster on typical hardware.

## How to use

Just compile and run:

```
gcc -o fscanfasta fscanfasta.c
./fscanfasta
```

The program will:
1. Generate a test file (`testdata.txt`) if it doesn't exist
2. Compare parsing speed using both methods

## Why?

Because sundays are boring.

## License

MIT, do whatever you want.