# Resurrect Skeleton

Given a CD image skeleton (built by [redumper](https://github.com/superg/redumper)), and the component files, this program will attempt to rebuild the original CD image.

```
resurrect.exe -f -r image.skeleton
```

`-f` or `--force` will continue to rebuild the skeleton even if a matching file is not found.
`-r` or `--recursive` will look within all subdirectories for matching files.

'''Note''': ECC is currently broken. Use CDmage or similar to fix the ECC. ISO rebuilding currently works fine.
