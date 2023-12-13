Reproducer for corruption, which is currently known to be triggered by interaction of disk writes and discards. 

WARNING #1: writes directly into the device, potentially corrupting any file system that lives there.
WARNING #2: Don't forget to unmount the file system if any, otherwise it can interfere with the reproducer.

To build, run: make

To run: sudo ./reproducer.sh (WARNING: don't forget to update WORKING_DEVICE in the script)

If corruption manifests, this will be dumped

```
Running blkx #1 at offset 2G with PID 6816 and writing output at ./blkx1.output
Running blkx #2 at offset 5G with PID 6817 and writing output at ./blkx2.output
BLKX failed; inspect blkx*.output files in /home/support/corruption_reproducer
```

Then proceed to inspect the log files which will show something as follow if corruption happened:

```
Reading at offset=5799997440, size=131072
Writing at offset=6322688000, size=131072
Reading at offset=6322688000, size=131072
Writing at offset=6308896768, size=131072
Reading at offset=6308896768, size=131072
Writing at offset=5795151872, size=131072
Reading at offset=5795151872, size=131072
Writing at offset=5668974592, size=131072
Reading at offset=5668974592, size=131072
File memory differs at offset=5669064704 ('g' != 'e')
Some file operations failed
```


