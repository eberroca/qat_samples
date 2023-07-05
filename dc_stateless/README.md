The following is sample code written in C showcasing QAT acceleration
to compress and decompress text data. This code is very simple, only using 
one QAT instance.

To compile, edit the file `build.sh` and change the variable `ICP_ROOT` to
point to where the QAT package is installed in the system. Then, simply, run:

```
$ ./build.sh
```

To execute the sample, run:

```
$ ./main
```

"El Quijote" in text form (used by the sample) is downloaded from:

https://gist.githubusercontent.com/jsdario/6d6c69398cb0c73111e49f1218960f79/raw/8d4fc4548d437e2a7203a5aeeace5477f598827d/el_quijote.txt
