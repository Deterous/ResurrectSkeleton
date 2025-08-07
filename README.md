# Resurrect Skeleton

Given a disc image skeleton (built by [redumper](https://github.com/superg/redumper)), and the component files, this program will attempt to rebuild the original disc image. Supports ISOs, Mode1 CD-ROMs, and Mode2Form1 CD-ROMs (no Mode2Form2 support yet).

```
resurrect.exe -f -r image.skeleton
```

`-f` or `--force` will continue to rebuild the skeleton even if a matching file is not found.
`-r` or `--recursive` will look within all subdirectories for matching files.

**Note**: ECC is currently broken for resurrecting CD skeletons. For MODE1, convert to ISO and back. For MODE2 Form1, running resurrect twice seems to fix this for now.
