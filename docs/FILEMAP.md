# File ID Mapping (Filemap)

## Why Filemap Exists

When `MT_FILE_ID_MODE=1`, the library stores only `file_id` (32-bit hash of `__FILE__`).
This is compact and deterministic, but the PC decoder needs a map to translate hashes back to file names.

## How It Works

- MCU snapshot contains: `file=0xHASH line=123`
- PC decoder receives `--filemap` flag:
  - Maps `0xHASH -> path/to/file.c`

## Usage

```bash
python tools/mt_decode.py dump.bin --filemap mt_filemap.txt
```

Format of `mt_filemap.txt` (one file per line):

```
0x6c9c16d2 src/net.c
0x11223344 drivers/spi.c
```

## File Stability (Critical)

Different build environments may produce different `__FILE__` strings (absolute vs relative paths).
Recommendation: Stabilize the prefix in your toolchain.

### GCC/Clang Tip

Use macro prefix mapping (example):

```bash
-fmacro-prefix-map=/abs/path/to/project=.
```

In CMake:

```cmake
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.)
```

## Debugging Missing Mappings

If the decoder can't find a hash in the filemap:

- It prints `file_id=0xHEXVALUE` (still useful for debugging)
- Add the missing file to your filemap and re-decode
