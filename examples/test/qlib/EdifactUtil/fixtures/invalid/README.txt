These fixtures are intentionally invalid EDMD files used to exercise strict loader errors.

Some invalid releases include empty UNCL/ directories with a .keep file so the loader
reaches the intended EDMD parsing errors instead of failing on missing directories.
