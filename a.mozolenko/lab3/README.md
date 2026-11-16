# Lab 3: User IDs and file access

Build and run on Solaris/Illumos:

```sh
make
ls -l lab3 data.txt
./lab3
```

The program prints the real and effective user IDs, opens `data.txt` with
`fopen(3)`, closes it with `fclose(3)`, calls `setuid(getuid())`, and repeats
the ID and file-access checks. File-open failures are reported with
`perror(3C)`.

The data file must be readable and writable only by its owner:

```sh
chmod 0600 data.txt
```

To test execution by another member of the group, set group execute and
directory search permissions as appropriate:

```sh
chmod 0750 lab3
chmod 0750 .
chmod 0600 data.txt
```

Without the set-user-ID bit, another group member should fail to open the
owner-only data file. As the executable owner, enable set-user-ID execution:

```sh
chmod u+s lab3
ls -l lab3 data.txt
```

With the bit enabled, the first check runs with the executable owner's
effective UID and can open the file. After `setuid(getuid())`, the effective
UID becomes the real caller UID, so the second check should fail for another
group member. Remove the bit after testing:

```sh
chmod u-s lab3
```
