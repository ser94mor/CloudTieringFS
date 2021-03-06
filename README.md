# CloudTieringFS: Policy-Driven Multi-Tier File System
The daemon and the shared library that together enable support of tiering
between a POSIX-compliant file system (local or distributed)
and a cloud object storage at the file level while maintaining the POSIX
semantics.

### Documentation
A very detailed description of the CloudTieringFS is given in 
[Sergey Morozov's Master's Thesis](docs/sergey-morozov-masters-thesis.pdf).

### Installation
To start daemon run:
```
$> sudo S3_ACCESS_KEY_ID='<access-key-id>' \
        S3_SECRET_ACCESS_KEY='<secret-access-key>' \
   $PWD/bin/cloudtiering-daemon $PWD/conf/cloudtiering.conf
```

Set `LD_PRELOAD` variable globally
```
$> export LD_PRELOAD=$PWD/bin/libcloudtiering.so
$> <executable>
```
or individually, per process
```
$> LD_PRELOAD=$PWD/bin/libcloudtiering.so <executable>
```


### Dependencies
Below is a list of tools and libraries that should be installed on the system
in order to enable code compilation. This list may be incomplete.
- *s3* shared library (openSUSE 42.2: `libs3-2`, `libs3-devel`);
- *dotconf* shared library (openSUSE 42.2: `dotconf`, `dotconf-devel`);
- *libattr* shared library (openSUSE 42.2: `libattr-devel`);
- *gcc5* compiler (openSUSE 42.2: `gcc5`);
- *proc* file system should be present on the system.


### Licensing
CloudTieringFS is licensed under the
[GNU General Public License, Version 3](LICENSE.md).
