# Reassembling split files

GitHub rejects any single file over 100 MB. Three files in this dump
exceed that limit and were split into ~90 MB chunks before pushing.
The chunks live next to where the original files were; the originals
are `.gitignore`d. Reassemble them with `cat`:

```bash
# Reassemble HiGHS static libraries (each ~192 MB)
cat lrsp_native/build/bin/highs.lib.part-* > lrsp_native/build/bin/highs.lib
cat mespprc_native/build/bin/highs.lib.part-* > mespprc_native/build/bin/highs.lib

# Reassemble the original Git pack file inside .git.bak (~140 MB)
cat .git.bak/objects/pack/pack-479498de1f5d66c08739d75722c0ee8f2863e397.pack.part-* \
    > .git.bak/objects/pack/pack-479498de1f5d66c08739d75722c0ee8f2863e397.pack
```

After reassembly the libraries are byte-identical to what was on disk
at the time of the upload. The chunks can be deleted if you want to
save space; they're only needed once to reconstruct the originals.

If you'd rather just rebuild the HiGHS libraries from source, the
vendored HiGHS code is in `mespprc_native/third_party/HiGHS/` — run
`mespprc_native/scripts/build.bat` and the .lib is produced fresh.
