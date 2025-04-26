# zq
[中文文档](README_cn.md)

## Introduction
Inspired by the open-source tool [zindex](https://github.com/mattgodbolt/zindex), this gzip log search tool is based on a partial rewrite of the original code.

The following major changes were made to the open-source version:
1. Completely rewrote the indexing logic to reduce the index file size. In the open-source version, the index file size is about 1.3 times that of the compressed log file. After optimization, the ratio is approximately 50:1.
2. Removed some parameters and features. The open-source version requires a config file to support multi-column indexing, while the modified version allows specifying multiple columns directly through parameters.

## Advantages

### Index File Size
The size of the index file depends on the data distribution and number of lines in the source file. High repetition in field values leads to smaller index files.

For example, indexing a 2GB compressed log file with 7 million lines and no duplicate field values produces an index file of about 36MB.

### Search Performance
Search performance is affected by the data distribution of the target field and the file size. Higher repetition in field values results in slower searches.

For the same file mentioned above:
- Using `zgrep` directly takes about 30+ seconds.
- Using the index-based search takes about 0.4 seconds.

## Usage

### zindex - Create index for compressed files
```
Usage: zindex [options] input-file

Options:
  -v, --verbose              Be more verbose
      --debug                Be even more verbose
      --colour, --color      Use colour even on non-TTY
  -w, --warnings             Log warnings at info level
      --checkpoint-every N   Create a compression checkpoint every N bytes
  -f, --field NUM            Create an index using field NUM (1-based)
  -d, --delimiter DELIM      Use DELIM as the field delimiter
      --tab-delimiter        Use a tab character as the field delimiter
```

### zq - Search in indexed compressed files
```
Usage: zq [options] query input-file [input-file...]

Options:
  -v, --verbose              Be more verbose
      --debug                Be even more verbose
      --colour, --color      Use colour even on non-TTY
  -w, --warnings             Log warnings at info level
  -i, --index INDEX          Use specified index for searching
```

## Principle

### Indexing

#### Chunk Index
The `seek` functionality within the gzip file is developed based on `zrand.c` by the xlib author. The basic logic involves splitting the gzip file into chunks by size. Each chunk requires a 32KB dictionary index, which allows direct seeking to the chunk's starting position for decompression. More details can be found in the background section of https://github.com/circulosmeos/gztool.

The chunk indexing logic from the open-source version is largely retained in this project. By default, a chunk is created every 32MB. The chunk's dictionary content is stored in an SQLite database. Internal indexing within the chunk is achieved through decompression and data dropping.

#### Field Index
A hash32 of the field value is taken and then modulo `0xFFFFu` to produce the index key. The gzip offset of the line containing the value is stored as the index value. The overall structure of the index data is a `hash<key, list<value>>`.

To save storage space, binary raw data is used with the following file format:
```
key  | size  | offset1 | offset2 | offset3
4byte| 4byte | 8byte   | 8byte   | 8byte
```

During a search, the file is read sequentially. If the key doesn't match, the next `size * 8` bytes are skipped, and the next key is read.

To further reduce the file size, offsets are compressed. Since each key maps to a list of offsets, if any offset exceeds `0xFFFFFFFE`, a special `mask` placeholder is inserted. Upon encountering a mask, subsequent offsets are calculated as:
```
offset = offset + 0xFFFFFFFE * count(mask)
```

Also, since the list size is no longer fixed-length, it’s not possible to drop lists precisely. To address this, a `padding` placeholder (`0xFFFFFFFF`) is added. When a key mismatch occurs, the process drops `size * 4` bytes and reads until the next `0xFFFFFFFF`.

Final index file format:
```
padding | key   | size  | offset1 | offset2 | mask  | offset3
4byte   | 4byte | 4byte | 4byte   | 4byte   | 4byte | 4byte
```

### Known Issues
1. Each query must sequentially read the entire file. However, since the index file is small, this doesn't significantly impact performance. For future optimization, keys could be placed at the file header during indexing, along with offsets to their corresponding value lists.
2. The size of the index file depends on the number of keys and the average length of the offset lists. Too many keys increase padding overhead and make offset compression ineffective. Too few keys increase collisions and the size of offset lists, requiring more chunk decompression. Currently, the maximum number of keys is 65,536, with no limit on offset list size. Future improvements could automatically adjust key and list sizes based on file content.


## Development
### Build
```bash
cmake . -DStatic:BOOL=On -DCMAKE_BUILD_TYPE=Release
make
```
