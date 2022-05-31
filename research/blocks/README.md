# blocks

## Introduction

Block_copy and Block_release are used to allocate and free memory blocks. They are not incredibly performant.

### Baseline, without blocks
  
```
mkdir -p build
clang -o build/string-baseline blocks/string-baseline.c -lexpress
time build/string-baseline
string no block!
        0.08 real         0.03 user         0.00 sys
```

### String object, with blocks

```
mkdir -p build
clang -o build/string-block blocks/string-block.c -lexpress
time build/string-block
string block!
        2.51 real         1.35 user         0.85 sys
```

## Conclusion

Our string object is about 25 times slower than the baseline.

## Thoughts

It is certainly much easier to work with our string object especially for complex string manipulations. Perhaps a good development practice would be to use a string object until performance is necessary and then to refactor the code to use bog-standard C strings.
