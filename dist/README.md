# Distribution installers

Store dependency installers and packaged X52 Battlefield Mapper application
installers in this directory. Keep versioned filenames so the exact package used
for a working setup can be retained. This folder is tracked by Git.

## Included

| File | Purpose | Size |
| --- | --- | --- |
| `X52_HOTAS_x64_8_0_213_0.exe` | Original X52 HOTAS x64 vendor installation package supplied from Downloads | 15,991,736 bytes |

Copied on 2026-09-21 from the user's Downloads folder. The source and repository
copy have matching SHA-256 hashes; see `SHA256SUMS`. Windows Authenticode verification
reported **Valid**, signed by **Logitech Inc**. The EXE reports 7-Zip 9.20 wrapper
metadata; the version in its filename is the package identifier, not a claim
about the version of every bundled driver component. The installer was not run.

Other dependency installers (such as a required .NET or Visual C++ runtime) and
the mapper's own application installer can be added here when available. They
are not included yet. Application data remains in `data` beside the running
mapper executable, separate from this installer archive.

Vendor binaries retain their original vendor licenses; the application's GPL
license does not replace those licenses.

## Verify the included installer

From the repository root, run:

```powershell
Get-FileHash -LiteralPath dist/X52_HOTAS_x64_8_0_213_0.exe -Algorithm SHA256
Get-AuthenticodeSignature -LiteralPath dist/X52_HOTAS_x64_8_0_213_0.exe
```
