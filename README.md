# Resurrect Skeleton

Given a disc image skeleton (built by [redumper](https://github.com/superg/redumper)), and the component files, this program will attempt to rebuild the original disc image. Supports ISOs, Mode1 CD-ROMs, and Mode2Form1 CD-ROMs (no Mode2Form2 support yet).

```
resurrect.exe -f -r image.skeleton
```

`-f` or `--force` will continue to rebuild the skeleton even if a matching file is not found.
`-r` or `--recursive` will look within all subdirectories for matching files.
